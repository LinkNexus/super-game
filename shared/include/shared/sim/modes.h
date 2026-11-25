#pragma once

#include "shared/helpers.h"
#include "shared/sim/messages.h"
#include "shared/sim/world.h"
#include <variant>

namespace shared::sim {
static inline void stepBullets(BulletsPool &bullets, const mathlib::Cuboidf &bounds, float dt) {
  for (auto &bullet : bullets) {
    if (bullet.active) {
      bullet.step(bounds, dt);
    }
  }
}

static inline void setBulletsPoolState(BulletsPoolState &state, BulletsPool &pool) {
  for (std::size_t idx = 0; idx < pool.size(); ++idx) {
    auto &bullet_state = state[idx];
    auto &bullet = pool[idx];

    bullet_state.position = bullet.position;
    bullet_state.type = bullet.type;
    bullet_state.active = bullet.active;
  }
}

template <World W>
class CoopGameSim {
private:
  CoopGamePhase phase_{CoopGamePhase::ENEMIES_ENTRANCE};
  EnemiesPoolSimState enemies_pool_{};
  BossSimState boss_{};
  std::array<std::optional<PlayerSimState>, MAX_PLAYERS_COOP> players_{};
  PlayerCount initial_player_count_{};
  BulletsPool bullets_pool_;
  W::FormationMotion formation_motion_{};
  W::BossMotion boss_motion_{};

  static constexpr PlayerLives INITIAL_PLAYER_LIVES = 3;

private:
  void setGameState(CoopGameState &state) {
    state.players.fill(std::nullopt);
    PlayerCount playerIdx{0};

    for (const auto &player : players_) {
      if (player) {
        auto &p = state.players[playerIdx++].emplace();
        p.position = player->position;
        p.lives = player->lives;
        p.points = player->points;
        p.id = player->id;
      }
    }

    state.phase = phase_;

    setBulletsPoolState(state.bullets, bullets_pool_);

    for (std::size_t idx = 0; enemies_pool_.enemies.size() && idx < state.enemies.size(); ++idx) {
      const auto &enemy = enemies_pool_.enemies[idx];
      state.enemies[idx] = {enemy.alive, enemy.type};
    }
    state.enemies_offset = enemies_pool_.offset;

    state.boss.position = boss_.position;
    state.boss.active = boss_.active;
    state.boss.health = boss_.health;
    state.boss.max_health = boss_.max_health;
  }

  void checkCollisions() {
    for (auto &bullet : bullets_pool_) {
      if (!bullet.active)
        continue;

      const auto bulletHitbox = bullet.getHitBox();

      if (bullet.type == BulletType::PLAYER) {
        if (phase_ == CoopGamePhase::FIGHT_BOSS && boss_.active &&
            mathlib::intersects(bulletHitbox, boss_.getHitBox())) {
          boss_.health--;

          for (auto &player : players_) {
            if (player && player->id == bullet.owner_id) {
              player->points += POINTS_PER_HIT;
              break;
            }
          }

          bullet.active = false;
        }

        else if (phase_ == CoopGamePhase::FIGHT_ENEMIES) {
          for (auto &enemy : enemies_pool_.enemies) {
            if (enemy.alive && mathlib::intersects(bulletHitbox, enemy.getHitBox())) {
              enemy.alive = false;
              bullet.active = false;

              for (auto &player : players_) {
                if (player && player->id == bullet.owner_id) {
                  player->points += POINTS_PER_HIT;
                  break;
                }
              }

              break;
            }
          }
        }
      } else if (bullet.type == BulletType::ENEMY) {
        for (auto &player : players_) {
          if (player && player->lives > 0 &&
              mathlib::intersects(bulletHitbox, player->getHitBox())) {
            player->lives--;
            bullet.active = false;
            break;
          }
        }
      }
    }
  }

public:
  void removePlayer(PlayerId playerId) {
    for (auto &player : players_) {
      if (player && player->id == playerId) {
        player = std::nullopt;
        break;
      }
    }

    if (phase_ != CoopGamePhase::WON && phase_ != CoopGamePhase::GAME_OVER &&
        count_optional(players_) == 0) {
      phase_ = CoopGamePhase::GAME_OVER;
    }
  }

  template <std::size_t N>
    requires(N > 0 && N <= MAX_PLAYERS_COOP)
  void start(const PlayerIds<N> &ids) {
    const auto count = count_optional(ids);

    if (count < 1)
      throw std::runtime_error("At least one player is required to start a coop game");

    initial_player_count_ = count;

    PlayerCount slot = 0;
    for (const auto &id : ids) {
      if (!id)
        continue;
      while (players_[slot])
        ++slot;
      players_[slot].emplace().init(*id, W::playerEntrance(0.0f), INITIAL_PLAYER_LIVES, 0.0f);
    }

    enemies_pool_.init(W::formationEntrance, initial_player_count_, W::formationSlot);

    boss_ = BossSimState();
    phase_ = CoopGamePhase::PLAYERS_ENTRANCE;
    bullets_pool_.fill(BulletSimState());
  }

  template <std::size_t N>
    requires(N > 0 && N <= MAX_PLAYERS_COOP)
  void step(CoopGameState &state, const PlayerInputs<N> &inputs, float dt) {
    auto allPlayersDead = true;
    auto canFire = phase_ == CoopGamePhase::FIGHT_ENEMIES || phase_ == CoopGamePhase::FIGHT_BOSS;

    const auto stepPlayers = [this, &inputs, &canFire, dt]() -> void {
      for (auto &player : players_) {
        if (player && player->lives > 0) {
          auto it = std::find_if(inputs.begin(), inputs.end(), [&](const auto &input) {
            return input && input->player_id == player->id;
          });

          if (it != inputs.end()) {
            const auto &input = **it;
            player->step(
                W::moveDirection(input.buttons),
                W::playerBounds(player->orientation),
                dt,
                bullets_pool_,
                buttonsHave(input.buttons, Button::SHOOT),
                canFire
            );
          }
        }
      }
    };

    switch (phase_) {
    case CoopGamePhase::PLAYERS_ENTRANCE: {
      for (auto &player : players_) {
        if (player)
          player->stepEntrance(dt);
      }

      for (const auto &player : players_) {
        if (player && !player->isEntranceComplete()) {
          setGameState(state);
          return;
        }
      }

      phase_ = CoopGamePhase::ENEMIES_ENTRANCE;
      break;
    }

    case CoopGamePhase::ENEMIES_ENTRANCE: {
      stepPlayers();
      enemies_pool_.stepEntrance(dt);

      if (enemies_pool_.isEntranceComplete())
        phase_ = CoopGamePhase::FIGHT_ENEMIES;

      break;
    }

    case CoopGamePhase::FIGHT_ENEMIES: {
      stepPlayers();
      const auto aliveExtents = enemies_pool_.aliveExtents();

      if (aliveExtents)
        enemies_pool_.step(
            W::moveFormation(formation_motion_, aliveExtents.value(), dt), dt, bullets_pool_
        );

      stepBullets(bullets_pool_, W::BULLET_BOUNDS, dt);

      checkCollisions();

      if (enemies_pool_.allEnemiesDefeated()) {
        boss_.init(W::BOSS_ENTRANCE, initial_player_count_);
        phase_ = CoopGamePhase::BOSS_ENTRANCE;
        break;
      }

      // Alive extends returns the cuboid that passes through the alive enemies
      // at the limits. But we need the box that encloses the whole alive
      // formation
      const auto enemiesBox = enemies_pool_.aliveExtents();
      if (enemiesBox &&
          mathlib::intersects(enemiesBox->resizedBy(EnemySimState::SIZE), W::playerBounds(0.0f))) {
        phase_ = CoopGamePhase::GAME_OVER;
      }

      break;
    }

    case CoopGamePhase::BOSS_ENTRANCE: {
      stepPlayers();
      boss_.stepEntrance(dt);
      stepBullets(bullets_pool_, W::BULLET_BOUNDS, dt);

      if (boss_.isEntranceComplete())
        phase_ = CoopGamePhase::FIGHT_BOSS;

      break;
    }

    case CoopGamePhase::FIGHT_BOSS: {
      stepPlayers();
      std::array<std::optional<mathlib::Vec3f>, MAX_PLAYERS_COOP> playersPositions{};
      std::transform(
          players_.begin(),
          players_.end(),
          playersPositions.begin(),
          [](const auto &player) -> std::optional<mathlib::Vec3f> {
            if (!player || player->lives <= 0)
              return std::nullopt;
            return player->position;
          }
      );

      boss_.step(
          W::bossBulletsSpreadDirections,
          W::moveBoss(boss_motion_, boss_.position, dt),
          playersPositions,
          dt,
          bullets_pool_
      );
      stepBullets(bullets_pool_, W::BULLET_BOUNDS, dt);

      checkCollisions();

      if (boss_.health <= 0)
        phase_ = CoopGamePhase::WON;

      break;
    }

    case CoopGamePhase::WON:
    case CoopGamePhase::GAME_OVER:
      setGameState(state);
      return;
    }

    for (const auto &player : players_) {
      if (player && player->lives > 0) {
        allPlayersDead = false;
        break;
      }
    }

    if (allPlayersDead)
      phase_ = CoopGamePhase::GAME_OVER;

    setGameState(state);
  }
};

using PvPGameTeamRosters =
    std::array<std::optional<std::pair<TeamId, PlayerIds<MAX_PLAYERS_PER_TEAM>>>, MAX_TEAMS>;

struct PvPGameTeam {
  TeamId id{};
  PvPGameTeamOutcome outcome{PvPGameTeamOutcome::PLAYING};
  std::array<std::optional<PlayerSimState>, MAX_PLAYERS_PER_TEAM> players{};
  float orientation{};

  void init(
      TeamId id,
      const PlayerIds<MAX_PLAYERS_PER_TEAM> playerIds,
      float playersSpacing,
      LinearEntrance entrance,
      float orientation,
      PlayerLives lives
  ) {
    this->id = id;
    this->orientation = orientation;

    const auto teamSize = count_optional(playerIds);
    const auto teamLength = teamSize * PlayerSimState::SIZE.x + (teamSize - 1) * playersSpacing;

    std::size_t slot = 0;
    for (auto &id : playerIds) {
      if (!id)
        continue;

      while (players[slot])
        slot++;

      const auto offset = mathlib::Vec3f{
          -teamLength / 2 + PlayerSimState::SIZE.x / 2 +
              slot * (PlayerSimState::SIZE.x + playersSpacing),
          0.0f,
          0.0f
      };

      const auto startingPos = entrance.trajectory.start + offset;
      players[slot].emplace().init(
          id.value(),
          {.trajectory = mathlib::LineSegment3f::fromStartAndDirection(
               startingPos, entrance.trajectory.direction()
           ),
           .speed = entrance.speed},
          lives,
          orientation
      );
    }
  }
};

template <World W>
class PvPGameSim {
private:
  std::array<std::optional<PvPGameTeam>, MAX_TEAMS> teams_{};
  PvPGamePhase phase_{PvPGamePhase::PLAYERS_FIGHT};
  BulletsPool bullets_pool_;

  static constexpr auto PLAYERS_SPACING = 30.0f;
  static constexpr int POINTS_PER_HIT = 50;
  static constexpr uint8_t INITIAL_LIVES = 10;

private:
  void setGameState(PvPGameState &state) {
    state.teams.fill(std::nullopt);
    std::size_t teamIdx = 0;

    for (const auto &team : teams_) {
      if (!team)
        continue;

      auto &t = state.teams[teamIdx++].emplace();
      t.id = team->id;
      t.outcome = team->outcome;

      std::size_t slot = 0;
      for (const auto &player : team->players) {
        if (!player)
          continue;

        auto &p = t.players[slot++].emplace();
        p.position = player->position;
        p.lives = player->lives;
        p.points = player->points;
        p.id = player->id;
        p.orientation = player->orientation;
      }
    }

    state.phase = phase_;
    setBulletsPoolState(state.bullets, bullets_pool_);
  }

  void checkCollisions() {
    for (auto &bullet : bullets_pool_) {
      if (!bullet.active)
        continue;

      PlayerSimState *ownerPlayer = nullptr;

      for (auto &team : teams_) {
        if (team) {
          for (auto &player : team->players) {
            if (player && player->id == bullet.owner_id) {
              ownerPlayer = &(*player);
              break;
            }
          }
        }

        if (ownerPlayer)
          break;
      }

      for (auto &team : teams_) {
        if (!team)
          continue;

        auto ownerIsInTeam =
            std::any_of(team->players.begin(), team->players.end(), [&bullet](const auto &player) {
              return player && player->id == bullet.owner_id;
            });

        if (ownerIsInTeam)
          continue;

        for (auto &player : team->players) {
          if (!player || player->lives <= 0 ||
              !mathlib::intersects(bullet.getHitBox(), player->getHitBox()))
            continue;

          if (ownerPlayer)
            ownerPlayer->points += POINTS_PER_HIT;

          player->lives--;
          bullet.active = false;
          break;
        }
      }
    }
  }

public:
  void start(const PvPGameTeamRosters &ids) {
    TeamCount teamSlot{0};

    const auto teamCount = count_optional(ids);
    const auto orientationStep = 2 * std::numbers::pi_v<float> / static_cast<float>(teamCount);
    float currentOrientation = 0.0f;

    for (const auto &kvp : ids) {
      if (!kvp)
        continue;

      auto &[rId, rPlayers] = *kvp;
      while (teams_[teamSlot])
        ++teamSlot;
      auto &team = teams_[teamSlot++].emplace();
      auto size = count_optional(rPlayers);

      if (size < 1)
        throw std::runtime_error(
            "A minimum of 1 player is required per team, given: " + std::to_string(size)
        );

      team.init(
          rId,
          rPlayers,
          PLAYERS_SPACING,
          W::playerEntrance(currentOrientation),
          currentOrientation,
          INITIAL_LIVES
      );

      currentOrientation += orientationStep;
    }

    phase_ = PvPGamePhase::PLAYERS_ENTRANCE;
    bullets_pool_.fill(BulletSimState());
  }

  template <std::size_t N>
    requires(N > 0 && N <= MAX_TEAMS * MAX_PLAYERS_PER_TEAM)
  void step(PvPGameState &state, const PlayerInputs<N> &inputs, float dt) {
    switch (phase_) {
    case PvPGamePhase::PLAYERS_ENTRANCE: {
      for (auto &team : teams_) {
        if (team) {
          for (auto &player : team->players) {
            if (player)
              player->stepEntrance(dt);
          }
        }
      }

      for (const auto &team : teams_) {
        if (!team)
          continue;
        for (const auto &player : team->players) {
          if (player && !player->isEntranceComplete()) {
            setGameState(state);
            return;
          }
        }
      }

      phase_ = PvPGamePhase::PLAYERS_FIGHT;
      break;
    };

    case PvPGamePhase::PLAYERS_FIGHT: {
      for (auto &team : teams_) {
        if (team) {
          for (auto &player : team->players) {
            if (player && player->lives > 0) {
              const auto it = std::find_if(inputs.begin(), inputs.end(), [&](const auto &input) {
                return input && input->player_id == player->id;
              });

              if (it != inputs.end()) {
                const auto &input = **it;
                player->step(
                    W::moveDirection(input.buttons),
                    W::playerBounds(player->orientation),
                    dt,
                    bullets_pool_,
                    buttonsHave(input.buttons, Button::SHOOT),
                    true
                );
              }
            }
          }
        }
      }

      stepBullets(bullets_pool_, W::BULLET_BOUNDS, dt);

      checkCollisions();

      std::array<bool, MAX_TEAMS> teamsAreDead{};
      teamsAreDead.fill(true);

      for (const auto &team : teams_) {
        if (!team)
          continue;

        auto isDead = true;
        for (const auto &player : team->players) {
          if (player && player->lives > 0) {
            isDead = false;
            break;
          }
        }

        teamsAreDead[team->id - 1] = isDead;
      }

      auto allTeamsDead =
          std::all_of(teams_.begin(), teams_.end(), [&teamsAreDead](const auto &team) {
            return team && teamsAreDead[team->id - 1];
          });

      if (allTeamsDead) {
        for (auto &team : teams_) {
          if (team)
            team->outcome = PvPGameTeamOutcome::DREW;
        }

        phase_ = PvPGamePhase::END;
      } else {
        TeamId winningTeamId = 0;

        for (const auto &team : teams_) {
          if (team && !teamsAreDead[team->id - 1]) {
            if (winningTeamId == 0)
              winningTeamId = team->id;
            else {
              winningTeamId = 0;
              break;
            }
          }
        }

        if (winningTeamId != 0) {
          for (auto &team : teams_) {
            if (team) {
              if (team->id == winningTeamId)
                team->outcome = PvPGameTeamOutcome::WON;
              else
                team->outcome = PvPGameTeamOutcome::LOST;
            }
          }

          phase_ = PvPGamePhase::END;
        }
      }

      break;
    }

    default:
      break;
    }

    setGameState(state);
  }

  void removePlayer(PlayerId playerId) {
    for (auto &team : teams_) {
      if (team) {
        for (auto &player : team->players) {
          if (player && player->id == playerId) {
            player = std::nullopt;
            break;
          }
        }
      }
    }
  }
};

using FlatCoop = CoopGameSim<Flat>;
using FlatPvP = PvPGameSim<Flat>;
using GameSim = std::variant<FlatCoop, FlatPvP>;
} // namespace shared::sim
