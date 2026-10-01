#include "shared/sim/game_sim.h"
#include "shared/aliases.h"
#include "shared/constants.h"
#include "shared/math_utils.h"
#include "shared/messages.h"
#include "shared/rnd_generator.h"
#include "shared/sim/boss_sim.h"
#include "shared/sim/bullet_sim.h"
#include "shared/sim/enemy_sim.h"
#include "shared/sim/player_sim.h"
#include <cstdint>

using namespace shared;

void CoopGameSim::init(PlayerCount playerCount) {
  if (playerCount < 1 || playerCount > MAX_PLAYERS_COOP)
    throw std::runtime_error("Too many players for CoopGameSim, given: " +
                             std::to_string(playerCount) +
                             ", max: " + std::to_string(MAX_PLAYERS_COOP));

  player_count_ = playerCount;
}

std::array<shared::PlayerId, MAX_PLAYERS_COOP> CoopGameSim::start() {
  for (std::size_t i = 0; i < player_count_; ++i) {
    players_[i].init(i + 1);
  }

  enemies_pool_.init(player_count_);
  boss_ = BossSimState();
  phase_ = Phase::ENEMIES_ENTRANCE;
  bullets_pool_.fill(BulletSimState());

  auto ids = std::array<shared::PlayerId, MAX_PLAYERS_COOP>{};
  std::generate(ids.begin(), ids.end(), [n = 0]() mutable { return ++n; });

  return ids;
}

void PvPGameSim::init(PlayerCount teamSize, PlayerCount teamCount) {
  std::string errMsg{};

  if (teamSize == 0 || teamSize > MAX_PLAYERS_PER_TEAM) {
    errMsg = "PvPGameSim: team_size must be between 1 and " +
             std::to_string(MAX_PLAYERS_PER_TEAM) +
             ", given: " + std::to_string(teamSize);
  }

  if (teamCount < 2 || teamCount > MAX_TEAMS) {
    errMsg = (errMsg.empty() ? "" : ", ") +
             std::string("PvPGameSim: team_count must be between 2 and " +
                         std::to_string(MAX_TEAMS) +
                         ", given: " + std::to_string(teamCount));
  }

  if (!errMsg.empty())
    throw std::runtime_error(errMsg);

  team_size_ = teamSize;
  team_count_ = teamCount;
}

std::array<std::array<PlayerId, MAX_PLAYERS_PER_TEAM>, MAX_TEAMS>
PvPGameSim::start() {
  auto playerIds =
      std::array<std::array<PlayerId, MAX_PLAYERS_PER_TEAM>, MAX_TEAMS>{};
  auto teamId = 1;
  auto playerId = 1;
  bool isOnTop = true;

  for (std::size_t teamIdx = 0; teamIdx < team_count_; ++teamIdx) {
    auto &team = teams_[teamIdx];
    team.init(teamId++, team_size_, isOnTop);

    auto positionX = (SCREEN_WIDTH - (PlayerSimState::SIZE * team_size_) -
                      ((team_size_ - 1) * PLAYERS_SPACING)) /
                     2.0f;

    for (std::size_t playerIdx = 0; playerIdx < team_size_; ++playerIdx) {
      auto &player = team.players[playerIdx];

      player.init(playerIds[teamIdx][playerIdx] = playerId++,
                  {positionX, team.initial_position_y}, INITIAL_LIVES,
                  isOnTop ? toRads(180) : 0);
      positionX += PlayerSimState::SIZE + PLAYERS_SPACING;
    }

    isOnTop = !isOnTop;
  }

  phase_ = Phase::PLAYERS_ENTRANCE;
  bullets_pool_.fill(BulletSimState());

  return playerIds;
}

template <auto MAX_PLAYERS_COUNT>
void stepPlayer(const std::array<PlayerInput, MAX_PLAYERS_COUNT> &inputs,
                PlayerSimState &player, BulletsPool &bullets_pool, float dt,
                bool canFire) {
  auto playerInput =
      std::find_if(inputs.begin(), inputs.end(), [&](const auto &input) {
        return input.player_id == player.id;
      });

  if (playerInput != inputs.end()) {
    player.step(*playerInput, dt, bullets_pool, canFire);
  }
}

void CoopGameSim::step(CoopGameState &state,
                       const std::array<PlayerInput, MAX_PLAYERS_COOP> &inputs,
                       float dt) {
  auto allPlayersDead = true;
  auto canFire = phase_ == Phase::FIGHT_ENEMIES || phase_ == Phase::FIGHT_BOSS;

  for (std::size_t i = 0; i < player_count_; ++i) {
    if (players_[i].lives > 0) {
      stepPlayer(inputs, players_[i], bullets_pool_, dt, canFire);
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

    for (auto &bullet : bullets_pool_) {
      if (bullet.active) {
        bullet.step(dt);
      }
    }

    checkCollisions();

    if (enemies_pool_.allEnemiesDefeated()) {
      phase_ = Phase::BOSS_ENTRANCE;
      boss_.init(player_count_);
    }

    if (enemies_pool_.reachedPlayer()) {
      phase_ = Phase::GAME_OVER;
    }
    break;

  case Phase::BOSS_ENTRANCE:
    boss_.stepEntrance(dt);

    for (auto &bullet : bullets_pool_) {
      if (bullet.active) {
        bullet.step(dt);
      }
    }

    if (boss_.isEntranceComplete()) {
      phase_ = Phase::FIGHT_BOSS;
    }
    break;

  case Phase::FIGHT_BOSS:
    boss_.step(dt, bullets_pool_);

    for (auto &bullet : bullets_pool_) {
      if (bullet.active) {
        bullet.step(dt);
      }
    }

    checkCollisions();

    if (boss_.health <= 0) {
      phase_ = Phase::WON;
    }

    break;

  case Phase::WON:
  case Phase::GAME_OVER:
    setGameState(state);
    return;
  }

  for (std::size_t i = 0; i < player_count_; ++i) {
    if (players_[i].lives > 0) {
      allPlayersDead = false;
      break;
    }
  }

  if (allPlayersDead) {
    phase_ = Phase::GAME_OVER;
  }

  setGameState(state);
}

void PvPGameSim::step(
    PvPGameState &state,
    const std::array<PlayerInput, MAX_TEAMS * MAX_PLAYERS_PER_TEAM> &inputs,
    float dt) {
  switch (phase_) {
  case Phase::PLAYERS_ENTRANCE: {
    auto allEntrancesCompleted = true;
    for (std::size_t i = 0; i < team_count_; ++i) {
      auto &team = teams_[i];

      team.stepEntrance(dt);

      allEntrancesCompleted =
          allEntrancesCompleted && team.isEntranceComplete();
    }

    if (allEntrancesCompleted) {
      phase_ = Phase::PLAYERS_FIGHT;
    }

    break;
  }

  case Phase::PLAYERS_FIGHT: {
    for (std::size_t teamIdx = 0; teamIdx < team_count_; ++teamIdx) {
      auto &team = teams_[teamIdx];

      for (std::size_t playerIdx = 0; playerIdx < team.size; ++playerIdx) {
        auto &player = team.players[playerIdx];

        if (player.lives > 0) {
          stepPlayer(inputs, player, bullets_pool_, dt, true);
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
    for (std::size_t teamIdx = 0; teamIdx < team_count_; ++teamIdx) {
      auto isDead = true;

      for (std::size_t playerIdx = 0; playerIdx < teams_[teamIdx].size;
           ++playerIdx) {
        if (teams_[teamIdx].players[playerIdx].lives > 0) {
          isDead = false;
          break;
        }
      }

      teamsStatus[teamIdx] = isDead;
    }

    auto allTeamsDead = true;
    for (std::size_t teamIdx = 0; teamIdx < team_count_; ++teamIdx) {
      if (!teamsStatus[teamIdx]) {
        allTeamsDead = false;
        break;
      }
    }

    if (allTeamsDead) {
      for (std::size_t teamIdx = 0; teamIdx < team_count_; ++teamIdx) {
        teams_[teamIdx].outcome = TeamOutcome::DREW;
      }
      phase_ = Phase::END;
    } else {
      std::size_t winningTeamIdx = MAX_TEAMS;
      for (std::size_t teamIdx = 0; teamIdx < team_count_; ++teamIdx) {
        if (!teamsStatus[teamIdx]) {
          if (winningTeamIdx == MAX_TEAMS) {
            winningTeamIdx = teamIdx;
          } else {
            winningTeamIdx = MAX_TEAMS;
            break;
          }
        }
      }

      if (winningTeamIdx != MAX_TEAMS) {
        for (std::size_t teamIdx = 0; teamIdx < team_count_; ++teamIdx) {
          if (teamIdx == winningTeamIdx) {
            teams_[teamIdx].outcome = TeamOutcome::WON;
          } else {
            teams_[teamIdx].outcome = TeamOutcome::LOST;
          }
        }
        phase_ = Phase::END;
      }
    }

    break;
  }

  case Phase::END: {
    break;
  }
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
  for (std::size_t idx = 0; idx < player_count_; ++idx) {
    auto &player = players_[idx];

    state.players[idx].position = player.position;
    state.players[idx].lives = player.lives;
    state.players[idx].points = player.points;
    state.players[idx].id = player.id;
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

  state.player_count = player_count_;
}

void PvPGameSim::setGameState(PvPGameState &state) {
  for (std::size_t team_idx = 0; team_idx < teams_.size(); ++team_idx) {
    auto &team = teams_[team_idx];
    state.teams[team_idx].id = team.id;
    state.teams[team_idx].outcome = static_cast<uint8_t>(team.outcome);
    state.teams[team_idx].size = team.size;

    for (std::size_t player_idx = 0; player_idx < team.players.size();
         ++player_idx) {
      auto &player = team.players[player_idx];

      state.teams[team_idx].players[player_idx].position = player.position;
      state.teams[team_idx].players[player_idx].lives = player.lives;
      state.teams[team_idx].players[player_idx].points = player.points;
      state.teams[team_idx].players[player_idx].id = player.id;
      state.teams[team_idx].players[player_idx].orientation =
          player.orientation;
    }
  }

  state.phase = static_cast<uint8_t>(phase_);
  state.teams_count = team_count_;
  state.team_size = team_size_;

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

          for (std::size_t i = 0; i < player_count_; ++i) {
            auto &player = players_[i];
            if (player.id == bullet.owner_id) {
              player.points += POINTS_PER_HIT;
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

            for (std::size_t i = 0; i < player_count_; ++i) {
              auto &player = players_[i];
              if (player.id == bullet.owner_id) {
                player.points += POINTS_PER_HIT;
                break;
              }
            }

            break;
          }
        }
      }
    } else if (bullet.type == BulletType::ENEMY) {
      for (std::size_t i = 0; i < player_count_; ++i) {
        auto &player = players_[i];
        if (player.lives > 0 &&
            rectIntersection(
                bullet.position, BulletSimState::WIDTH / 2,
                BulletSimState::HEIGHT / 2, player.position,
                PlayerSimState::SIZE * PlayerSimState::HITBOX_SCALE / 2,
                PlayerSimState::SIZE * PlayerSimState::HITBOX_SCALE / 2)) {
          player.lives--;
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

    for (std::size_t teamIdx = 0; teamIdx < team_count_; ++teamIdx) {
      auto &team = teams_[teamIdx];

      for (std::size_t playerIdx = 0; playerIdx < team.size; ++playerIdx) {
        auto &player = team.players[playerIdx];

        if (player.id == bullet.owner_id) {
          ownerPlayer = &player;
          break;
        }
      }

      if (ownerPlayer)
        break;
    }

    for (std::size_t teamIdx = 0; teamIdx < team_count_; ++teamIdx) {
      auto &team = teams_[teamIdx];
      auto ownerIsInTeam = std::any_of(
          team.players.begin(), team.players.end(),
          [&](const auto &player) { return player.id == bullet.owner_id; });

      if (ownerIsInTeam)
        continue;

      for (std::size_t playerIdx = 0; playerIdx < team.size; ++playerIdx) {
        auto &player = team.players[playerIdx];

        if (player.lives <= 0)
          continue;

        if (rectIntersection(
                bullet.position, BulletSimState::WIDTH / 2,
                BulletSimState::HEIGHT / 2, player.position,
                PlayerSimState::SIZE * PlayerSimState::HITBOX_SCALE / 2,
                PlayerSimState::SIZE * PlayerSimState::HITBOX_SCALE / 2)) {
          if (ownerPlayer) {
            ownerPlayer->points += PvPGameSim::POINTS_PER_HIT;
          }

          player.lives--;
          bullet.active = false;
          break;
        }
      }
    }
  }
}

void PvPGameSim::Team::init(TeamId id, PlayerCount size, bool isOnTop) {
  this->id = id;
  this->size = size;
  is_on_top = isOnTop;
  initial_position_y =
      isOnTop ? (SCREEN_HEIGHT + INITIAL_OFFSET_Y) : -INITIAL_OFFSET_Y;
}

void PvPGameSim::Team::stepEntrance(float dt) {
  for (std::size_t i = 0; i < size; ++i) {
    auto &player = players[i];

    if (is_on_top) {
      if (player.position.y > (SCREEN_HEIGHT - PlayerSimState::POSITION_Y)) {
        player.position.y =
            std::max(player.position.y - ENTRANCE_SPEED * dt,
                     SCREEN_HEIGHT - PlayerSimState::POSITION_Y);
      }
    } else {
      if (player.position.y < PlayerSimState::POSITION_Y) {
        player.position.y = std::min(player.position.y + ENTRANCE_SPEED * dt,
                                     PlayerSimState::POSITION_Y);
      }
    }
  }
}

bool shared::PvPGameSim::Team::isEntranceComplete() const {
  return std::all_of(
      players.begin(), players.begin() + size, [&](const auto &p) {
        if (is_on_top) {
          return p.position.y <= (SCREEN_HEIGHT - PlayerSimState::POSITION_Y);
        } else {
          return p.position.y >= PlayerSimState::POSITION_Y;
        }
      });
}

void shared::PvPGameSim::removePlayer(PlayerId player_id) {
  for (std::size_t teamIdx = 0; teamIdx < team_count_; ++teamIdx) {
    auto &team = teams_[teamIdx];

    auto it = std::find_if(
        team.players.begin(), team.players.begin() + team.size,
        [&](const auto &player) { return player.id == player_id; });
    auto playerIdx = std::distance(team.players.begin(), it);

    if (playerIdx < 0 || playerIdx >= team.size)
      continue;

    team.players[playerIdx] = std::move(team.players[--team.size]);
  }
}

void shared::CoopGameSim::removePlayer(PlayerId player_id) {
  auto playerIt =
      std::find_if(players_.begin(), players_.end(),
                   [&](const auto &player) { return player.id == player_id; });
  auto playerIdx = std::distance(players_.begin(), playerIt);

  if (playerIdx < 0 || playerIdx >= player_count_)
    return;

  players_[playerIdx] = std::move(players_[--player_count_]);

  if (phase_ != Phase::WON && phase_ != Phase::GAME_OVER &&
      player_count_ == 0) {
    phase_ = Phase::GAME_OVER;
  }
}
