#include "shared/sim/game_sim.h"
#include "shared/constants.h"
#include "shared/helpers.h"
#include "shared/messages.h"
#include "shared/sim/boss_sim.h"
#include "shared/sim/bullet_sim.h"
#include "shared/sim/enemy_sim.h"
#include "shared/sim/player_sim.h"
#include <cstddef>
#include <cstdint>
#include <mathlib.hpp>
#include <utility>

using namespace shared;

void shared::PvPGameSim::start(const TeamRosters &ids) {
  auto isOnTop{true};
  TeamCount teamSlot{0};

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
          "A minimum of 1 player is required per team, given: " +
          std::to_string(size));

    team.init(rId,
              isOnTop ? (SCREEN_HEIGHT - PlayerSimState::POSITION_Y)
                      : PlayerSimState::POSITION_Y,
              isOnTop ? mathlib::toRadians(180.0f) : 0);
    auto positionX = (SCREEN_WIDTH - (PlayerSimState::SIZE * size) -
                      ((size - 1) * PLAYERS_SPACING)) /
                     2.0f;

    PlayerCount playerSlot{0};
    for (const auto &id : rPlayers) {
      if (!id)
        continue;

      while (team.players[playerSlot])
        playerSlot++;

      auto &player = team.players[playerSlot++].emplace();
      player.init(id.value(), {positionX, team.initial_position_y},
                  INITIAL_LIVES, team.orientation);

      positionX += PlayerSimState::SIZE + PLAYERS_SPACING;
    }

    isOnTop = !isOnTop;
    phase_ = PvPGamePhase::PLAYERS_FIGHT;
    bullets_pool_.fill(BulletSimState());
  }
}

void setBulletsPoolState(BulletsPoolState &state, BulletsPool &pool) {
  for (std::size_t idx = 0; idx < pool.size(); ++idx) {
    auto &bullet_state = state[idx];
    auto &bullet = pool[idx];

    bullet_state.position = bullet.position;
    bullet_state.type = bullet.type;
    bullet_state.active = bullet.active;
  }
}

void CoopGameSim::setGameState(CoopGameState &state) {
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

  for (std::size_t idx = 0;
       enemies_pool_.enemies.size() && idx < state.enemies.size(); ++idx) {
    const auto &enemy = enemies_pool_.enemies[idx];
    state.enemies[idx] = {enemy.alive, enemy.type};
  }
  state.enemies_offset = enemies_pool_.offset;

  state.boss.position = boss_.position;
  state.boss.active = boss_.active;
  state.boss.health = boss_.health;
  state.boss.max_health = boss_.max_health;
}

void PvPGameSim::setGameState(PvPGameState &state) {
  state.teams.fill(std::nullopt);
  PlayerCount teamIdx{0};

  for (const auto &team : teams_) {
    if (team) {
      auto &t = state.teams[teamIdx++].emplace();
      t.id = team->id;
      t.outcome = team->outcome;

      PlayerCount playerIdx{0};
      for (const auto &player : team->players) {
        if (player) {
          auto &p = t.players[playerIdx++].emplace();
          p.position = player->position;
          p.lives = player->lives;
          p.points = player->points;
          p.id = player->id;
          p.orientation = player->orientation;
        }
      }
    }
  }

  state.phase = phase_;

  setBulletsPoolState(state.bullets, bullets_pool_);
}

void CoopGameSim::checkCollisions() {
  for (auto &bullet : bullets_pool_) {
    if (!bullet.active)
      continue;

    const auto bulletHitbox = bullet.getHitBox();

    if (bullet.type == BulletType::PLAYER) {
      if (phase_ == CoopGamePhase::FIGHT_BOSS) {
        if (boss_.active &&
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
      }

      else if (phase_ == CoopGamePhase::FIGHT_ENEMIES) {
        for (auto &enemy : enemies_pool_.enemies) {
          if (enemy.alive &&
              mathlib::intersects(bulletHitbox, enemy.getHitBox())) {
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

void PvPGameSim::checkCollisions() {
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
      if (team) {
        auto ownerIsInTeam =
            std::any_of(team->players.begin(), team->players.end(),
                        [&](const auto &player) {
                          return player && player->id == bullet.owner_id;
                        });

        if (ownerIsInTeam)
          continue;

        for (auto &player : team->players) {
          if (player && player->lives > 0 &&
              mathlib::intersects(bullet.getHitBox(), player->getHitBox())) {
            if (ownerPlayer) {
              ownerPlayer->points += POINTS_PER_HIT;
            }

            player->lives--;
            bullet.active = false;
            break;
          }
        }
      }
    }
  }
}

void PvPGameSim::Team::init(TeamId id, float positionY, float orientation) {
  this->id = id;
  this->orientation = orientation;
  initial_position_y = positionY;
}

void shared::PvPGameSim::removePlayer(PlayerId player_id) {
  for (auto &team : teams_) {
    if (team) {
      for (auto &player : team->players) {
        if (player && player->id == player_id) {
          player = std::nullopt;
          break;
        }
      }
    }
  }
}

void shared::CoopGameSim::removePlayer(PlayerId player_id) {
  for (auto &player : players_) {
    if (player && player->id == player_id) {
      player = std::nullopt;
      break;
    }
  }

  if (phase_ != CoopGamePhase::WON && phase_ != CoopGamePhase::GAME_OVER &&
      count_optional(players_) == 0) {
    phase_ = CoopGamePhase::GAME_OVER;
  }
}
