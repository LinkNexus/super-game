#pragma once

#include "shared/constants.h"
#include "shared/messages.h"
#include "shared/sim/boss_sim.h"
#include "shared/sim/enemy_sim.h"
#include "shared/sim/player_sim.h"

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

  void start(PlayerIds &player_ids);
  void step(CoopGameState &state, const PlayerInputs &inputs, float dt);
  void removePlayer(PlayerId player_id);

private:
  Phase phase_{Phase::ENEMIES_ENTRANCE};
  EnemiesPoolSimState enemies_pool_{};
  BossSimState boss_{};
  OptionalTypeInPlayerSlots<PlayerSimState> players_{};
  uint8_t players_count_{0};
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
    std::array<std::optional<PlayerSimState>, MAX_PLAYERS / 2> players{};
    bool is_on_top{};
    float initial_position_y{};

    void init(TeamId id, bool isOnTop);
    void stepEntrance(float dt);
    bool isEntranceComplete() const;

    static constexpr float INITIAL_OFFSET_Y = 20.0f;
    static constexpr float ENTRANCE_SPEED = 50.0f;
  };

  enum class Phase : uint8_t { PLAYERS_ENTRANCE, PLAYERS_FIGHT, END };

  using Teams = std::array<Team, 2>;
  using PerTeamPlayerIds =
      std::array<std::array<std::optional<PlayerId>, MAX_PLAYERS / 2>, 2>;

  PvPGameSim(std::size_t team_size);
  void start(PerTeamPlayerIds &team_players_ids);
  void step(PvPGameState &state, const PlayerInputs &inputs, float dt);
  void removePlayer(PlayerId player_id);

  static constexpr float PLAYERS_SPACING = 30.0f;
  static constexpr int POINTS_PER_HIT = 50;
  static constexpr int INITIAL_LIVES = 10;

private:
  std::size_t team_size_{};
  Teams teams_{};
  Phase phase_{Phase::PLAYERS_ENTRANCE};
  BulletsPool bullets_pool_;

private:
  void setGameState(PvPGameState &state);
  void checkCollisions();
};

using GameSim = std::variant<CoopGameSim, PvPGameSim>;
} // namespace shared
