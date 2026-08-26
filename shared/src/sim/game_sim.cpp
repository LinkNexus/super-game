#include "shared/sim/game_sim.h"
#include "shared/constants.h"
#include "shared/helpers.h"
#include "shared/math_utils.h"
#include "shared/messages.h"
#include "shared/rnd_generator.h"
#include "shared/sim/boss_sim.h"
#include "shared/sim/bullet_sim.h"
#include "shared/sim/enemy_sim.h"
#include "shared/sim/player_sim.h"
#include <cstdint>
#include <optional>

using namespace shared;

void CoopGameSim::start(PlayerIds &players_ids) {
  players_count_ = 0;
  for (std::size_t idx = 0; idx < players_.size(); ++idx) {
    if (players_ids[idx].has_value()) {
      players_count_++;
      players_[idx].emplace();
      players_[idx]->init(players_ids[idx].value());
    }
  }

  enemies_pool_.init(players_count_);

  boss_ = BossSimState();
  phase_ = Phase::ENEMIES_ENTRANCE;
  bullets_pool_.fill(BulletSimState());
}

void PvPGameSim::start(PerTeamPlayerIds &team_players_ids) {
  auto id = 1;
  bool isOnTop = true;

  for (std::size_t team_idx = 0; team_idx < teams_.size(); ++team_idx) {
    auto &team = teams_[team_idx];
    team.init(id, isOnTop);
    isOnTop = !isOnTop;

    auto position_x = (SCREEN_WIDTH - (PlayerSimState::SIZE * team_size_) -
                       ((team_size_ - 1) * PLAYERS_SPACING)) /
                      2.0f;

    for (std::size_t player_idx = 0; player_idx < team.players.size();
         ++player_idx) {
      if (team_players_ids[team_idx][player_idx].has_value()) {
        auto &player = team.players[player_idx];
        player.emplace();

        player->init(team_players_ids[team_idx][player_idx].value(),
                     {position_x, team.initial_position_y}, INITIAL_LIVES);
        position_x += PlayerSimState::SIZE + PLAYERS_SPACING;
      }
    }
  }

  phase_ = Phase::PLAYERS_ENTRANCE;
  bullets_pool_.fill(BulletSimState());
}

void stepPlayer(const PlayerInputs &inputs, PlayerSimState &player,
                BulletsPool &bullets_pool, float dt, bool canFire) {
  auto player_input =
      std::find_if(inputs.begin(), inputs.end(), [&](const auto &i) {
        return i.has_value() && i->player_id == player.id;
      });

  if (player_input != inputs.end() && player_input->has_value()) {
    player.step(player_input->value(), dt, bullets_pool, canFire);
  }
}

void CoopGameSim::step(CoopGameState &state, const PlayerInputs &inputs,
                       float dt) {
  auto allPlayersDead = true;
  auto canFire = phase_ == Phase::FIGHT_ENEMIES || phase_ == Phase::FIGHT_BOSS;

  for (auto &player : players_) {
    if (!player.has_value())
      continue;

    if (player->lives > 0) {
      stepPlayer(inputs, player.value(), bullets_pool_, dt, canFire);
    }
  }

  switch (phase_) {
  case Phase::ENEMIES_ENTRANCE:
    enemies_pool_.stepEntrance(dt);

    if (enemies_pool_.isEntranceComplete()) {
      phase_ = Phase::FIGHT_ENEMIES;
    }
    break;

  case Phase::FIGHT_ENEMIES:
    enemies_pool_.step(dt, bullets_pool_);

    if (enemies_pool_.allEnemiesDefeated()) {
      phase_ = Phase::BOSS_ENTRANCE;
      boss_.init(players_count_);
    }

    if (enemies_pool_.reachedPlayer()) {
      phase_ = Phase::GAME_OVER;
    }
    break;

  case Phase::BOSS_ENTRANCE:
    boss_.stepEntrance(dt);

    if (boss_.isEntranceComplete()) {
      phase_ = Phase::FIGHT_BOSS;
    }
    break;

  case Phase::FIGHT_BOSS:
    boss_.step(dt, bullets_pool_);

    if (boss_.health <= 0) {
      phase_ = Phase::WON;
    }

    break;

  case Phase::WON:
  case Phase::GAME_OVER:
    setGameState(state);
    return;
  }

  for (auto &bullet : bullets_pool_) {
    if (bullet.active) {
      bullet.step(dt);
    }
  }

  if (phase_ == Phase::FIGHT_ENEMIES || phase_ == Phase::FIGHT_BOSS)
    checkCollisions();

  for (auto &player : players_) {
    if (player.has_value() && player->lives > 0) {
      allPlayersDead = false;
      break;
    }
  }

  if (allPlayersDead) {
    phase_ = Phase::GAME_OVER;
    setGameState(state);
    return;
  }

  setGameState(state);
}

void PvPGameSim::step(PvPGameState &state, const PlayerInputs &inputs,
                      float dt) {
  auto canFire = phase_ == Phase::PLAYERS_FIGHT;

  for (auto &team : teams_) {
    for (auto &player : team.players) {
      if (player.has_value() && player->lives > 0) {
        stepPlayer(inputs, player.value(), bullets_pool_, dt, canFire);
      }
    }
  }

  switch (phase_) {
  case Phase::PLAYERS_ENTRANCE:
    for (auto &team : teams_) {
      team.stepEntrance(dt);
    }

    if (std::all_of(teams_.begin(), teams_.end(), [](const auto &team) {
          return team.isEntranceComplete();
        })) {
      phase_ = Phase::PLAYERS_FIGHT;
    }

    break;
  case Phase::PLAYERS_FIGHT: {
    for (auto &bullet : bullets_pool_) {
      if (bullet.active) {
        bullet.step(dt);
      }
    }

    if (phase_ == Phase::PLAYERS_FIGHT)
      checkCollisions();

    auto isTeamDead = [](const Team &team) {
      return std::all_of(team.players.begin(), team.players.end(),
                         [](const auto &player) {
                           return !player.has_value() || player->lives <= 0;
                         });
    };

    const bool team0Dead = isTeamDead(teams_[0]);
    const bool team1Dead = isTeamDead(teams_[1]);

    if (team0Dead && team1Dead) {
      teams_[0].outcome = TeamOutcome::DREW;
      teams_[1].outcome = TeamOutcome::DREW;
      phase_ = Phase::END;
    } else if (team0Dead || team1Dead) {
      const std::size_t deadIdx = team0Dead ? 0 : 1;
      teams_[deadIdx].outcome = TeamOutcome::LOST;
      teams_[1 - deadIdx].outcome = TeamOutcome::WON;
      phase_ = Phase::END;
    }

    break;
  }
  case Phase::END:
    setGameState(state);
    return;
  }

  setGameState(state);
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
  for (std::size_t idx = 0; idx < players_.size(); ++idx) {
    auto &player = players_[idx];

    if (!player.has_value())
      continue;

    if (!state.players[idx].has_value())
      state.players[idx].emplace();

    state.players[idx]->position = player->position;
    state.players[idx]->lives = player->lives;
    state.players[idx]->points = player->points;
    state.players[idx]->id = player->id;
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
  for (std::size_t team_idx = 0; team_idx < teams_.size(); ++team_idx) {
    auto &team = teams_[team_idx];
    state.teams[team_idx].id = team.id;
    state.teams[team_idx].outcome = static_cast<uint8_t>(team.outcome);

    for (std::size_t player_idx = 0; player_idx < team.players.size();
         ++player_idx) {
      auto &player = team.players[player_idx];

      if (!player.has_value())
        continue;

      if (!state.teams[team_idx].players[player_idx].has_value())
        state.teams[team_idx].players[player_idx].emplace();

      state.teams[team_idx].players[player_idx]->position = player->position;
      state.teams[team_idx].players[player_idx]->lives = player->lives;
      state.teams[team_idx].players[player_idx]->points = player->points;
      state.teams[team_idx].players[player_idx]->id = player->id;
    }
  }

  state.phase = static_cast<uint8_t>(phase_);

  setBulletsPoolState(state.bullets, bullets_pool_);
}

void CoopGameSim::checkCollisions() {
  for (auto &bullet : bullets_pool_) {

    if (bullet.active) {
      if (bullet.type == BulletType::PLAYER) {
        if (phase_ == Phase::FIGHT_BOSS) {
          if (boss_.active &&
              rectIntersection(bullet.position, BulletSimState::WIDTH / 2,
                               BulletSimState::HEIGHT / 2, boss_.position,
                               BossSimState::WIDTH / 2,
                               BossSimState::HEIGHT / 2)) {

            boss_.health--;
            for (auto &player : players_) {
              if (player.has_value() && player->id == bullet.owner_id) {
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
                if (player.has_value() && player->id == bullet.owner_id) {
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
          if (player.has_value() && player->lives > 0 &&
              rectIntersection(
                  bullet.position, BulletSimState::WIDTH / 2,
                  BulletSimState::HEIGHT / 2, player->position,
                  PlayerSimState::SIZE * PlayerSimState::HITBOX_SCALE / 2,
                  PlayerSimState::SIZE * PlayerSimState::HITBOX_SCALE / 2)) {
            player->lives--;
            bullet.active = false;
          }
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
      for (auto &player : team.players) {
        if (player.has_value() && player->id == bullet.owner_id) {
          ownerPlayer = &player.value();
          break;
        }
      }
      if (ownerPlayer)
        break;
    }

    for (auto &team : teams_) {
      auto ownerIsInTeam = std::any_of(
          team.players.begin(), team.players.end(), [&](const auto &player) {
            return player.has_value() && player->id == bullet.owner_id;
          });

      if (ownerIsInTeam)
        continue;

      for (auto &player : team.players) {
        if (!player.has_value() || player->lives <= 0)
          continue;

        if (rectIntersection(
                bullet.position, BulletSimState::WIDTH / 2,
                BulletSimState::HEIGHT / 2, player->position,
                PlayerSimState::SIZE * PlayerSimState::HITBOX_SCALE / 2,
                PlayerSimState::SIZE * PlayerSimState::HITBOX_SCALE / 2)) {
          if (ownerPlayer) {
            ownerPlayer->points += PvPGameSim::POINTS_PER_HIT;
          }

          player->lives--;
          bullet.active = false;
          break;
        }
      }
    }
  }
}

shared::PvPGameSim::PvPGameSim(std::size_t team_size) {
  if (team_size == 0 || team_size > MAX_PLAYERS / 2) {
    throw std::invalid_argument(
        "PvPGameSim: team_size must be between 1 and MAX_PLAYERS/2");
  }

  team_size_ = team_size;
}

void PvPGameSim::Team::init(TeamId id, bool isOnTop) {
  this->id = id;
  is_on_top = isOnTop;
  initial_position_y =
      isOnTop ? -INITIAL_OFFSET_Y : (SCREEN_HEIGHT + INITIAL_OFFSET_Y);
}

void PvPGameSim::Team::stepEntrance(float dt) {
  for (auto &p : players) {
    if (p) {
      if (is_on_top) {
        if (p->position.y < (SCREEN_HEIGHT - PlayerSimState::POSITION_Y)) {
          p->position.y =
              std::min(p->position.y + ENTRANCE_SPEED * dt,
                       (SCREEN_HEIGHT - PlayerSimState::POSITION_Y));
        }
      } else {
        if (p->position.y > PlayerSimState::POSITION_Y) {
          p->position.y = std::max(p->position.y - ENTRANCE_SPEED * dt,
                                   PlayerSimState::POSITION_Y);
        }
      }
    }
  }
}

bool shared::PvPGameSim::Team::isEntranceComplete() const {
  return std::all_of(players.begin(), players.end(), [&](const auto &p) {
    if (!p)
      return true;

    if (is_on_top) {
      return p->position.y >= (SCREEN_HEIGHT - PlayerSimState::POSITION_Y);
    } else {
      return p->position.y <= PlayerSimState::POSITION_Y;
    }
  });
}
