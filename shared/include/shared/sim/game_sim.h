#pragma once

#include "shared/aliases.h"
#include "shared/constants.h"
#include "shared/messages.h"
#include "shared/sim/boss_sim.h"
#include "shared/sim/enemy_sim.h"
#include "shared/sim/player_sim.h"
#include <cstdint>

namespace shared {
class CoopGameSim {
public:
  enum class Phase : uint8_t {
    ENEMIES_ENTRANCE,
    FIGHT_ENEMIES,
    BOSS_ENTRANCE,
    FIGHT_BOSS,
    WON,
    GAME_OVER
  };

  void init(PlayerCount playerCount);

  std::array<shared::PlayerId, MAX_PLAYERS_COOP> start();

  void step(CoopGameState &state,
            const std::array<shared::PlayerInput, MAX_PLAYERS_COOP> &inputs,
            float dt);

  void removePlayer(PlayerId player_id);

private:
  Phase phase_{Phase::ENEMIES_ENTRANCE};
  EnemiesPoolSimState enemies_pool_{};
  BossSimState boss_{};
  std::array<PlayerSimState, MAX_PLAYERS_COOP> players_{};
  PlayerCount player_count_{0};
  BulletsPool bullets_pool_;

private:
  void setGameState(CoopGameState &state);
  void checkCollisions();
};

class PvPGameSim {
public:
  enum class TeamOutcome { PLAYING, WON, LOST, DREW };

  using TeamId = uint8_t;

  struct Team {
    TeamId id{};
    TeamOutcome outcome{TeamOutcome::PLAYING};
    std::array<PlayerSimState, MAX_PLAYERS_PER_TEAM> players{};
    uint8_t size{};
    bool is_on_top{};
    float initial_position_y{};

    void init(TeamId id, PlayerCount size, bool isOnTop);
    void stepEntrance(float dt);
    bool isEntranceComplete() const;

    static constexpr float INITIAL_OFFSET_Y = 20.0f;
    static constexpr float ENTRANCE_SPEED = 50.0f;
  };

  enum class Phase : uint8_t { PLAYERS_ENTRANCE, PLAYERS_FIGHT, END };

  void init(PlayerCount teamSize, PlayerCount teamsCount);

  std::array<std::array<PlayerId, MAX_PLAYERS_PER_TEAM>, MAX_TEAMS> start();

  void
  step(PvPGameState &state,
       const std::array<PlayerInput, MAX_TEAMS * MAX_PLAYERS_PER_TEAM> &inputs,
       float dt);

  void removePlayer(PlayerId player_id);

  static constexpr float PLAYERS_SPACING = 30.0f;
  static constexpr int POINTS_PER_HIT = 50;
  static constexpr int INITIAL_LIVES = 10;

private:
  uint8_t team_count_{};
  uint8_t team_size_{};
  std::array<Team, MAX_TEAMS> teams_{};
  Phase phase_{Phase::PLAYERS_ENTRANCE};
  BulletsPool bullets_pool_;

private:
  void setGameState(PvPGameState &state);
  void checkCollisions();
};

using GameSim = std::variant<CoopGameSim, PvPGameSim>;
} // namespace shared
