#include "shared/sim/game_sim.h"
#include "shared/aliases.h"
#include "shared/constants.h"
#include "shared/helpers.h"
#include "shared/math_utils.h"
#include "shared/messages.h"
#include "shared/rnd_generator.h"
#include "shared/sim/boss_sim.h"
#include "shared/sim/bullet_sim.h"
#include "shared/sim/enemy_sim.h"
#include "shared/sim/player_sim.h"
#include <cstddef>
#include <cstdint>

using namespace shared;

void shared::PvPGameSim::start(const TeamRosters &ids) {
  bool isOnTop{true};
  PlayerCount teamSlot{0};

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
              isOnTop ? toRads(180) : 0);
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

      isOnTop = !isOnTop;
    }

    phase_ = Phase::PLAYERS_FIGHT;
    bullets_pool_.fill(BulletSimState());
  }
}

void setBulletsPoolState(BulletsPoolState &state, BulletsPool &pool) {
  for (std::size_t idx = 0; idx < pool.size(); ++idx) {
    auto &bullet_state = state[idx];
    auto &bullet = pool[idx];

    bullet_state.position = bullet.position;
    bullet_state.type = static_cast<uint8_t>(bullet.type);
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

  state.phase = static_cast<uint8_t>(phase_);

  setBulletsPoolState(state.bullets, bullets_pool_);

  for (std::size_t idx = 0;
       enemies_pool_.enemies.size() && idx < state.enemies.size(); ++idx) {
    state.enemies[idx][0] = enemies_pool_.enemies[idx].alive ? 1 : 0;
    state.enemies[idx][1] =
        static_cast<uint8_t>(enemies_pool_.enemies[idx].type);
  }
  state.enemies_offset_x = enemies_pool_.offset_x;
  state.enemies_offset_y = enemies_pool_.offset_y;

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
      t.outcome = static_cast<uint8_t>(team->outcome);

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

  state.phase = static_cast<uint8_t>(phase_);

  setBulletsPoolState(state.bullets, bullets_pool_);
}

void CoopGameSim::checkCollisions() {
  for (auto &bullet : bullets_pool_) {
    if (!bullet.active)
      continue;

    if (bullet.type == BulletType::PLAYER) {
      if (phase_ == Phase::FIGHT_BOSS) {
        if (boss_.active &&
            rectIntersection(bullet.position, BulletSimState::WIDTH / 2,
                             BulletSimState::HEIGHT / 2, boss_.position,
                             BossSimState::WIDTH / 2,
                             BossSimState::HEIGHT / 2)) {

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

      else if (phase_ == Phase::FIGHT_ENEMIES) {
        for (auto &enemy : enemies_pool_.enemies) {
          if (enemy.alive &&
              rectIntersection(bullet.position, BulletSimState::WIDTH / 2,
                               BulletSimState::HEIGHT / 2, enemy.position,
                               EnemySimState::WIDTH / 2,
                               EnemySimState::HEIGHT / 2)) {
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
            rectIntersection(
                bullet.position, BulletSimState::WIDTH / 2,
                BulletSimState::HEIGHT / 2, player->position,
                PlayerSimState::SIZE * PlayerSimState::HITBOX_SCALE / 2,
                PlayerSimState::SIZE * PlayerSimState::HITBOX_SCALE / 2)) {
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
              rectIntersection(
                  bullet.position, BulletSimState::WIDTH / 2,
                  BulletSimState::HEIGHT / 2, player->position,
                  PlayerSimState::SIZE * PlayerSimState::HITBOX_SCALE / 2,
                  PlayerSimState::SIZE * PlayerSimState::HITBOX_SCALE / 2)) {
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

  if (phase_ != Phase::WON && phase_ != Phase::GAME_OVER &&
      count_optional(players_) == 0) {
    phase_ = Phase::GAME_OVER;
  }
}
