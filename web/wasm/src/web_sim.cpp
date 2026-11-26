#include "shared/rnd_generator.h"
#include "shared/sim/constants.h"
#include "shared/sim/entities/bullet.h"
#include "shared/sim/entities/player.h"
#include "shared/sim/messages.h"
#include "shared/sim/modes.h"
#include "shared/sim/types.h"
#include "shared/sim/world.h"
#include "shared/wire.h"
#include <emscripten/bind.h>
#include <mathlib.hpp>

using namespace shared::sim;

static constexpr PlayerCount MAX_PLAYERS_ON_THIS_MACHINE = 2;

class WebSim {
private:
  GameSim sim_;
  GameState state_;
  PlayerIds<MAX_PLAYERS_ON_THIS_MACHINE> player_ids_{};

public:
  WebSim() { shared::RndGenerator::seed(); }

  template <World W>
  void startCoop(PlayerCount playerCount) {
    if (playerCount == 0)
      throw std::runtime_error("Player count must be greater than 0");

    if (playerCount > MAX_PLAYERS_ON_THIS_MACHINE)
      throw std::runtime_error(
          "Player count exceeds " + std::to_string(MAX_PLAYERS_ON_THIS_MACHINE)
      );

    player_ids_.fill(std::nullopt);
    auto &s = sim_.emplace<CoopGameSim<W>>();

    for (std::size_t i = 0; i < playerCount; ++i) {
      auto id = PlayerId(i + 1);
      player_ids_[i] = id;
    }

    s.start(player_ids_);
    state_ = CoopGameState();
  }

  void startCoop2D(PlayerCount playerCount) { startCoop<FlatWorld>(playerCount); }

  template <World W>
  void startPvP(PlayerCount teamCount, PlayerCount teamSize) {
    if (teamCount == 0 || teamSize == 0)
      throw std::runtime_error("Team count and team size must be greater than 0");

    if (teamCount > MAX_TEAMS)
      throw std::runtime_error("Team count exceeds " + std::to_string(MAX_TEAMS));

    if (teamSize > MAX_PLAYERS_PER_TEAM)
      throw std::runtime_error("Team size exceeds " + std::to_string(MAX_PLAYERS_PER_TEAM));

    if (teamCount * teamSize > MAX_PLAYERS_ON_THIS_MACHINE)
      throw std::runtime_error(
          "Total players exceeds " + std::to_string(MAX_PLAYERS_ON_THIS_MACHINE) +
          " (teamCount * teamSize = " + std::to_string(teamCount * teamSize) + ")"
      );

    player_ids_.fill(std::nullopt);
    auto &s = sim_.emplace<PvPGameSim<W>>();

    PvPGameTeamRosters ids{};
    PlayerId currentId{1};

    for (std::size_t i = 0; i < teamCount; ++i) {
      auto &[rId, rPlayers] = ids[i].emplace();
      rId = i + 1;

      for (std::size_t j = 0; j < teamSize; ++j) {
        rPlayers[j] = currentId++;
        player_ids_[i * teamSize + j] = rPlayers[j];
      }
    }

    s.start(ids);
    state_ = PvPGameState();
  }

  void startPvP2D(PlayerCount teamCount, PlayerCount teamSize) {
    startPvP<FlatWorld>(teamCount, teamSize);
  }

  std::string step(std::string inputPayload) {
    auto inputs = shared::decode(inputPayload, shared::WireFormat::JSON)
                      .get<PlayerInputs<MAX_PLAYERS_ON_THIS_MACHINE>>();

    std::string envelope;

    std::visit(
        shared::overloaded{
            [&inputs, &envelope]<World W>(CoopGameSim<W> &sim, CoopGameState &state) {
              sim.step(state, inputs, FIXED_DT);
              envelope = shared::encode(state, shared::WireFormat::JSON);
            },
            [&inputs, &envelope]<World W>(PvPGameSim<W> &sim, PvPGameState &state) {
              sim.step(state, inputs, FIXED_DT);
              envelope = shared::encode(state, shared::WireFormat::JSON);
            },
            [](auto &, auto &) { throw std::runtime_error("Invalid sim/state variant"); }
        },
        sim_,
        state_
    );

    return envelope;
  }

  std::string getPlayerIds() const { return shared::encode(player_ids_, shared::WireFormat::JSON); }
};

EMSCRIPTEN_BINDINGS(supergame) {
  emscripten::constant("MAX_PLAYERS_ON_THIS_MACHINE", MAX_PLAYERS_ON_THIS_MACHINE);
  emscripten::constant("MAX_PLAYERS_PER_CLIENT", MAX_PLAYERS_PER_CLIENT);
  emscripten::constant("FIXED_DT", FIXED_DT);
  emscripten::constant("MAX_PLAYERS_COOP", MAX_PLAYERS_COOP);
  emscripten::constant("MAX_TEAMS", MAX_TEAMS);
  emscripten::constant("MAX_PLAYERS_PER_TEAM", MAX_PLAYERS_PER_TEAM);

  emscripten::value_object<mathlib::Vec3f>("Vec3")
      .field("x", &mathlib::Vec3f::x)
      .field("y", &mathlib::Vec3f::y)
      .field("z", &mathlib::Vec3f::z);

  emscripten::constant("FLAT_WORLD_FIELD_BOUNDS_MIN", FlatWorld::FIELD_BOUNDS.min);
  emscripten::constant("FLAT_WORLD_FIELD_BOUNDS_MAX", FlatWorld::FIELD_BOUNDS.max);

  emscripten::constant("SIM_PLAYER_SIZE", PlayerSimState::SIZE);

  emscripten::constant("SIM_BULLET_SIZE", BulletSimState::SIZE);

  emscripten::constant("SIM_ENEMY_SIZE", EnemySimState::SIZE);
  emscripten::constant("SIM_ENEMIES_POOL_SPACING_X", EnemiesPoolSimState::SPACING_X);
  emscripten::constant("SIM_ENEMIES_POOL_SPACING_Z", EnemiesPoolSimState::SPACING_Z);
  emscripten::constant("SIM_ENEMY_COLS", ENEMY_COLS);
  emscripten::constant("SIM_MAX_ENEMY_ROWS", MAX_ENEMY_ROWS);

  emscripten::constant("SIM_BOSS_SIZE", BossSimState::SIZE);

  emscripten::constant("MAX_USERNAME_LENGTH", MAX_NAME_LENGTH);

  emscripten::enum_<PvPGamePhase>("PvPGamePhase", emscripten::enum_value_type::number)
      .value("PLAYERS_ENTRANCE", PvPGamePhase::PLAYERS_ENTRANCE)
      .value("PLAYERS_FIGHT", PvPGamePhase::PLAYERS_FIGHT)
      .value("END", PvPGamePhase::END);

  emscripten::enum_<CoopGamePhase>("CoopGamePhase", emscripten::enum_value_type::number)
      .value("PLAYERS_ENTRANCE", CoopGamePhase::PLAYERS_ENTRANCE)

      .value("ENEMIES_ENTRANCE", CoopGamePhase::ENEMIES_ENTRANCE)
      .value("FIGHT_ENEMIES", CoopGamePhase::FIGHT_ENEMIES)
      .value("BOSS_ENTRANCE", CoopGamePhase::BOSS_ENTRANCE)
      .value("FIGHT_BOSS", CoopGamePhase::FIGHT_BOSS)
      .value("WON", CoopGamePhase::WON)
      .value("GAME_OVER", CoopGamePhase::GAME_OVER);

  emscripten::enum_<ServerMessageType>("ServerMessageType", emscripten::enum_value_type::number)
      .value("WELCOME", ServerMessageType::WELCOME)
      .value("COOP_GAME_LOBBY_UPDATE", ServerMessageType::COOP_GAME_LOBBY_UPDATE)
      .value("PVP_GAME_LOBBY_UPDATE", ServerMessageType::PVP_GAME_LOBBY_UPDATE)
      .value("PVP_GAME_STATE", ServerMessageType::PVP_GAME_STATE)
      .value("COOP_GAME_STATE", ServerMessageType::COOP_GAME_STATE);

  emscripten::enum_<ClientMessageType>("ClientMessageType", emscripten::enum_value_type::number)
      .value("PLAYER_INPUT", ClientMessageType::PLAYER_INPUT)
      .value("READY", ClientMessageType::READY)
      .value("SWITCH_TEAM", ClientMessageType::SWITCH_TEAM);

  emscripten::enum_<Button>("Button", emscripten::enum_value_type::number)
      .value("NONE", Button::NONE)
      .value("LEFT", Button::LEFT)
      .value("RIGHT", Button::RIGHT)
      .value("UP", Button::UP)
      .value("DOWN", Button::DOWN)
      .value("SHOOT", Button::SHOOT);

  emscripten::class_<FlatWorld>("FlatWorld")
      .class_function("formationSlot", &FlatWorld::formationSlot);

  emscripten::class_<WebSim>("WebSim")
      .constructor<>()
      .function("startCoop2D", &WebSim::startCoop2D)
      .function("startPvP2D", &WebSim::startPvP2D)
      .function("step", &WebSim::step)
      .function("getPlayerIds", &WebSim::getPlayerIds);
}
