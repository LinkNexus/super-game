#pragma once

#include "shared/constants.h"
#include "shared/helpers.h"
#include "shared/sim/boss_sim.h"
#include "shared/sim/enemy_sim.h"
#include "shared/sim/player_sim.h"
#include "shared/types.h"
#include <cstdint>

namespace shared {
template <auto MAX_PLAYERS_COUNT>
static inline void stepPlayer(const PlayerInputs<MAX_PLAYERS_COUNT> &inputs,
                              PlayerSimState &player, BulletsPool &bullets_pool,
                              float dt, bool canFire) {
  auto playerInput =
      std::find_if(inputs.begin(), inputs.end(), [&](const auto &input) {
        return input && input->player_id == player.id;
      });

  if (playerInput != inputs.end()) {
    player.step(static_cast<Button>((*playerInput)->buttons), dt, bullets_pool,
                canFire);
  }
}

class CoopGameSim {
public:
  void removePlayer(PlayerId player_id);

private:
  CoopGamePhase phase_{CoopGamePhase::ENEMIES_ENTRANCE};
  EnemiesPoolSimState enemies_pool_{};
  BossSimState boss_{};
  std::array<std::optional<PlayerSimState>, MAX_PLAYERS_COOP> players_{};
  PlayerCount initial_player_count_{};
  BulletsPool bullets_pool_;

private:
  void setGameState(CoopGameState &state);
  void checkCollisions();

public:
  template <std::size_t N>
    requires(N > 0 && N <= MAX_PLAYERS_COOP)
  void start(const PlayerIds<N> &ids) {
    auto count = count_optional(ids);

    if (count < 1)
      throw std::runtime_error(
          "At least one player is required to start a coop game");

    initial_player_count_ = count;

    auto slot = 0;
    for (const auto &id : ids) {
      if (id) {
        while (players_[slot])
          ++slot;
        players_[slot++].emplace().init(*id);
      }
    }

    enemies_pool_.init(initial_player_count_);
    boss_ = BossSimState();
    phase_ = CoopGamePhase::ENEMIES_ENTRANCE;
    bullets_pool_.fill(BulletSimState());
  }

  template <std::size_t N>
    requires(N > 0 && N <= MAX_PLAYERS_COOP)
  void step(CoopGameState &state, const PlayerInputs<N> &inputs, float dt) {
    auto allPlayersDead = true;
    auto canFire = phase_ == CoopGamePhase::FIGHT_ENEMIES ||
                   phase_ == CoopGamePhase::FIGHT_BOSS;

    for (auto &player : players_) {
      if (player && player->lives > 0) {
        stepPlayer(inputs, *player, bullets_pool_, dt, canFire);
      }
    }

    switch (phase_) {
    case CoopGamePhase::ENEMIES_ENTRANCE:
      enemies_pool_.stepEntrance(dt);

      if (enemies_pool_.isEntranceComplete()) {
        phase_ = CoopGamePhase::FIGHT_ENEMIES;
      }
      break;

    case CoopGamePhase::FIGHT_ENEMIES:
      enemies_pool_.step(dt, bullets_pool_);

      for (auto &bullet : bullets_pool_) {
        if (bullet.active) {
          bullet.step(dt);
        }
      }

      checkCollisions();

      if (enemies_pool_.allEnemiesDefeated()) {
        phase_ = CoopGamePhase::BOSS_ENTRANCE;
        boss_.init(initial_player_count_);
      }

      if (enemies_pool_.reachedPlayer()) {
        phase_ = CoopGamePhase::GAME_OVER;
      }
      break;

    case CoopGamePhase::BOSS_ENTRANCE:
      boss_.stepEntrance(dt);

      for (auto &bullet : bullets_pool_) {
        if (bullet.active) {
          bullet.step(dt);
        }
      }

      if (boss_.isEntranceComplete()) {
        phase_ = CoopGamePhase::FIGHT_BOSS;
      }
      break;

    case CoopGamePhase::FIGHT_BOSS:
      boss_.step(dt, bullets_pool_);

      for (auto &bullet : bullets_pool_) {
        if (bullet.active) {
          bullet.step(dt);
        }
      }

      checkCollisions();

      if (boss_.health <= 0) {
        phase_ = CoopGamePhase::WON;
      }

      break;

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

class PvPGameSim {
public:
  using TeamRosters = std::array<
      std::optional<std::pair<TeamId, PlayerIds<MAX_PLAYERS_PER_TEAM>>>,
      MAX_TEAMS>;

  struct Team {
    TeamId id{};
    TeamOutcome outcome{TeamOutcome::PLAYING};
    std::array<std::optional<PlayerSimState>, MAX_PLAYERS_PER_TEAM> players{};
    float orientation{};
    float initial_position_y{};

    void init(TeamId id, float positionY, float orientation);

    static constexpr float ENTRANCE_SPEED = 50.0f;
  };

  void start(const TeamRosters &ids);

  void removePlayer(PlayerId player_id);

private:
  std::array<std::optional<Team>, MAX_TEAMS> teams_{};
  PvPGamePhase phase_{PvPGamePhase::PLAYERS_FIGHT};
  BulletsPool bullets_pool_;

  static constexpr auto PLAYERS_SPACING = 30.0f;
  static constexpr int POINTS_PER_HIT = 50;
  static constexpr uint8_t INITIAL_LIVES = 10;

private:
  void setGameState(PvPGameState &state);
  void checkCollisions();

public:
  template <std::size_t N>
    requires(N > 0 && N <= MAX_TEAMS * MAX_PLAYERS_PER_TEAM)
  void step(PvPGameState &state, const PlayerInputs<N> &inputs, float dt) {
    switch (phase_) {
    case PvPGamePhase::PLAYERS_FIGHT: {
      for (auto &team : teams_) {
        if (team) {
          for (auto &player : team->players) {
            if (player && player->lives > 0) {
              stepPlayer(inputs, *player, bullets_pool_, dt, true);
            }
          }
        }
      }

      for (auto &bullet : bullets_pool_) {
        if (bullet.active) {
          bullet.step(dt);
        }
      }

      checkCollisions();

      std::array<bool, MAX_TEAMS> teamsStatus{};
      teamsStatus.fill(true);

      for (const auto &team : teams_) {
        bool isDead{true};

        if (team) {
          for (const auto &player : team->players) {
            if (player && player->lives > 0) {
              isDead = false;
              break;
            }
          }

          teamsStatus[team->id - 1] = isDead;
        }
      }

      auto allTeamsDead = true;
      for (const auto &team : teams_) {
        if (team && !teamsStatus[team->id - 1]) {
          allTeamsDead = false;
          break;
        }
      }

      if (allTeamsDead) {
        for (auto &team : teams_) {
          if (team) {
            team->outcome = TeamOutcome::DREW;
          }
        }

        phase_ = PvPGamePhase::END;
      } else {
        TeamId winningTeamId{0};

        for (auto &team : teams_) {
          if (team && !teamsStatus[team->id - 1]) {
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
              if (team->id == winningTeamId) {
                team->outcome = TeamOutcome::WON;
              } else {
                team->outcome = TeamOutcome::LOST;
              }
            }
          }

          phase_ = PvPGamePhase::END;
        }
      }

      break;
    }

    case PvPGamePhase::END: {
      break;
    }
    }

    setGameState(state);
  }
};

using GameSim = std::variant<CoopGameSim, PvPGameSim>;
} // namespace shared
