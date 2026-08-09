#include "shared/sim/game_sim.h"
#include "shared/constants.h"
#include "shared/math_utils.h"
#include "shared/rnd_generator.h"
#include "shared/sim/boss_sim.h"
#include "shared/sim/bullet_sim.h"
#include "shared/sim/enemy_sim.h"
#include "shared/sim/player_sim.h"
#include <cstdint>
#include <optional>

using namespace shared;

void GameSim::start(
    std::array<std::optional<uint8_t>, MAX_PLAYERS> &player_ids) {
  RndGenerator::seed();

  for (std::size_t idx = 0; idx < players_.size(); ++idx) {
    if (player_ids[idx].has_value()) {
      players_[idx].emplace();
      players_[idx]->init(player_ids[idx].value());
    }
  }

  bullets_pool_.fill(BulletSimState());

  enemies_pool_.init();
  boss_ = BossSimState();
  phase_ = GamePhase::ENEMIES_ENTRANCE;
}

void GameSim::step(
    GameState &state,
    const std::array<std::optional<PlayerInput>, MAX_PLAYERS> &inputs,
    float dt) {
  bool can_fire_bullets =
      phase_ == GamePhase::FIGHT_ENEMIES || phase_ == GamePhase::FIGHT_BOSS;

  bool all_players_dead = true;
  std::array<std::optional<Vec2D>, MAX_PLAYERS> player_positions;

  for (std::size_t idx = 0; idx < players_.size(); ++idx) {
    auto &player = players_[idx];

    if (!player.has_value())
      continue;

    if (player->lives > 0) {
      all_players_dead = false;
      player_positions[idx] = player->position;

      auto player_input =
          std::find_if(inputs.begin(), inputs.end(), [&](const auto &i) {
            return i.has_value() && i->player_id == player->id;
          });

      if (player_input != inputs.end() && player_input->has_value()) {
        player->step(player_input->value(), dt, bullets_pool_,
                     can_fire_bullets);
      }
    }
  }

  if (all_players_dead) {
    phase_ = GamePhase::GAME_OVER;
    setGameState(state);
    return;
  }

  switch (phase_) {
  case GamePhase::ENEMIES_ENTRANCE:
    enemies_pool_.stepEntrance(dt);

    if (enemies_pool_.isEntranceComplete()) {
      phase_ = GamePhase::FIGHT_ENEMIES;
    }
    break;

  case GamePhase::FIGHT_ENEMIES:
    enemies_pool_.step(dt, bullets_pool_);

    if (enemies_pool_.allEnemiesDefeated()) {
      phase_ = GamePhase::BOSS_ENTRANCE;
      boss_.init();
    }

    if (enemies_pool_.reachedPlayer()) {
      phase_ = GamePhase::GAME_OVER;
    }
    break;

  case GamePhase::BOSS_ENTRANCE:
    boss_.stepEntrance(dt);

    if (boss_.isEntranceComplete()) {
      phase_ = GamePhase::FIGHT_BOSS;
    }
    break;

  case GamePhase::FIGHT_BOSS:
    boss_.step(dt, bullets_pool_, player_positions);

    if (boss_.health <= 0) {
      phase_ = GamePhase::WON;
    }

    break;

  case GamePhase::WON:
  case GamePhase::GAME_OVER:
    setGameState(state);
    return;
  }

  for (auto &bullet : bullets_pool_) {
    if (bullet.active) {
      bullet.step(dt);
    }
  }

  if (phase_ == GamePhase::FIGHT_ENEMIES || phase_ == GamePhase::FIGHT_BOSS)
    checkCollisions();

  setGameState(state);
}

void GameSim::setGameState(GameState &state) {
  state.tick++;

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

  for (std::size_t idx = 0; idx < bullets_pool_.size(); ++idx) {
    auto &bullet_state = state.bullets[idx];
    auto &bullet = bullets_pool_[idx];

    bullet_state.position = bullet.position;
    bullet_state.type = static_cast<uint8_t>(bullet.type);
    bullet_state.active = bullet.active;
  }

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
}

void GameSim::checkCollisions() {
  for (auto &bullet : bullets_pool_) {

    if (bullet.active) {
      if (bullet.type == BulletType::PLAYER) {
        if (phase_ == GamePhase::FIGHT_BOSS) {
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

        else if (phase_ == GamePhase::FIGHT_ENEMIES) {
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
