#include "shared/aliases.h"
#include "shared/constants.h"
#include "shared/math_utils.h"
#include "shared/messages.h"
#include "shared/rnd_generator.h"
#include "shared/sim/bullet_sim.h"
#include "shared/sim/game_sim.h"
#include "shared/sim/player_sim.h"
#include "shared/wire.h"
#include <emscripten/bind.h>

static constexpr shared::PlayerCount MAX_PLAYERS_ON_THIS_MACHINE = 2;

class WebSim {
private:
  shared::GameSim sim_;
  shared::GameState state_;
  shared::PlayerIds<MAX_PLAYERS_ON_THIS_MACHINE> player_ids_{};

public:
  void startCoop(shared::PlayerCount playerCount) {
    if (playerCount == 0)
      throw std::runtime_error("Player count must be greater than 0");

    if (playerCount > MAX_PLAYERS_ON_THIS_MACHINE)
      throw std::runtime_error("Player count exceeds " +
                               std::to_string(MAX_PLAYERS_ON_THIS_MACHINE));

    player_ids_.fill(std::nullopt);
    auto &s = sim_.emplace<shared::CoopGameSim>();

    for (std::size_t i = 0; i < playerCount; ++i) {
      auto id = shared::PlayerId(i + 1);
      player_ids_[i] = id;
    }

    s.start(player_ids_);
    state_ = shared::CoopGameState();
  }

  void startPvP(shared::PlayerCount teamCount, shared::PlayerCount teamSize) {
    if (teamCount == 0 || teamSize == 0)
      throw std::runtime_error(
          "Team count and team size must be greater than 0");

    if (teamCount > shared::MAX_TEAMS)
      throw std::runtime_error("Team count exceeds " +
                               std::to_string(shared::MAX_TEAMS));

    if (teamSize > shared::MAX_PLAYERS_PER_TEAM)
      throw std::runtime_error("Team size exceeds " +
                               std::to_string(shared::MAX_PLAYERS_PER_TEAM));

    if (teamCount * teamSize > MAX_PLAYERS_ON_THIS_MACHINE)
      throw std::runtime_error("Total players exceeds " +
                               std::to_string(MAX_PLAYERS_ON_THIS_MACHINE) +
                               " (teamCount * teamSize = " +
                               std::to_string(teamCount * teamSize) + ")");

    player_ids_.fill(std::nullopt);
    auto &s = sim_.emplace<shared::PvPGameSim>();

    shared::PvPGameSim::TeamRosters ids{};
    shared::PlayerId currentId{1};

    for (std::size_t i = 0; i < teamCount; ++i) {
      auto &[rId, rPlayers] = ids[i].emplace();
      rId = i + 1;

      for (std::size_t j = 0; j < teamSize; ++j) {
        rPlayers[j] = currentId++;
        player_ids_[i * teamSize + j] = rPlayers[j];
      }
    }

    s.start(ids);
    state_ = shared::PvPGameState();
  }

  std::string step(std::string inputPayload) {
    auto inputs = shared::decode(inputPayload)
                      .get<shared::PlayerInputs<MAX_PLAYERS_ON_THIS_MACHINE>>();

    std::string envelope;

    std::visit(shared::overloaded{
                   [&inputs, &envelope](shared::CoopGameSim &sim,
                                        shared::CoopGameState &state) {
                     sim.step(state, inputs, shared::FIXED_DT);
                     envelope = shared::encode(state);
                   },
                   [&inputs, &envelope](shared::PvPGameSim &sim,
                                        shared::PvPGameState &state) {
                     sim.step(state, inputs, shared::FIXED_DT);
                     envelope = shared::encode(state);
                   },
                   [](auto &, auto &) {
                     throw std::runtime_error("Invalid sim/state variant");
                   }},
               sim_, state_);

    return envelope;
  }

  std::string getPlayerIds() const { return shared::encode(player_ids_); }
};

EMSCRIPTEN_BINDINGS(supergame) {
  emscripten::constant("MAX_PLAYERS_ON_THIS_MACHINE",
                       MAX_PLAYERS_ON_THIS_MACHINE);
  emscripten::constant("FIXED_DT", shared::FIXED_DT);
  emscripten::constant("MAX_PLAYERS_COOP", shared::MAX_PLAYERS_COOP);
  emscripten::constant("MAX_TEAMS", shared::MAX_TEAMS);
  emscripten::constant("MAX_PLAYERS_PER_TEAM", shared::MAX_PLAYERS_PER_TEAM);

  emscripten::constant("SCREEN_WIDTH", shared::SCREEN_WIDTH);
  emscripten::constant("SCREEN_HEIGHT", shared::SCREEN_HEIGHT);

  emscripten::constant("SIM_PLAYER_SIZE", shared::PlayerSimState::SIZE);

  emscripten::constant("SIM_BULLET_HEIGHT", shared::BulletSimState::HEIGHT);
  emscripten::constant("SIM_BULLET_WIDTH", shared::BulletSimState::WIDTH);

  emscripten::constant("SIM_ENEMY_HEIGHT", shared::EnemySimState::HEIGHT);
  emscripten::constant("SIM_ENEMY_WIDTH", shared::EnemySimState::WIDTH);
  emscripten::constant("SIM_ENEMIES_POOL_SPACING_X",
                       shared::EnemiesPoolSimState::SPACING_X);
  emscripten::constant("SIM_ENEMIES_POOL_SPACING_Y",
                       shared::EnemiesPoolSimState::SPACING_Y);
  emscripten::constant("SIM_ENEMIES_POOL_COLS",
                       shared::EnemiesPoolSimState::COLS);
  emscripten::constant("SIM_ENEMIES_POOL_MAX_ROWS",
                       shared::EnemiesPoolSimState::MAX_ROWS);

  emscripten::constant("SIM_BOSS_HEIGHT", shared::BossSimState::HEIGHT);
  emscripten::constant("SIM_BOSS_WIDTH", shared::BossSimState::WIDTH);

  // To be called once at the TS code entry point
  emscripten::function("rndGeneratorSeed", &shared::RndGenerator::seed);

  emscripten::enum_<shared::PvPGameSim::Phase>(
      "PvPGamePhase", emscripten::enum_value_type::number)
      .value("PLAYERS_FIGHT", shared::PvPGameSim::Phase::PLAYERS_FIGHT)
      .value("END", shared::PvPGameSim::Phase::END);

  emscripten::enum_<shared::CoopGameSim::Phase>(
      "CoopGamePhase", emscripten::enum_value_type::number)
      .value("ENEMIES_ENTRANCE", shared::CoopGameSim::Phase::ENEMIES_ENTRANCE)
      .value("FIGHT_ENEMIES", shared::CoopGameSim::Phase::FIGHT_ENEMIES)
      .value("BOSS_ENTRANCE", shared::CoopGameSim::Phase::BOSS_ENTRANCE)
      .value("FIGHT_BOSS", shared::CoopGameSim::Phase::FIGHT_BOSS)
      .value("WON", shared::CoopGameSim::Phase::WON)
      .value("GAME_OVER", shared::CoopGameSim::Phase::GAME_OVER);

  emscripten::enum_<shared::ServerMessageType>(
      "ServerMessageType", emscripten::enum_value_type::number)
      .value("WELCOME", shared::ServerMessageType::WELCOME)
      .value("COOP_GAME_LOBBY_UPDATE",
             shared::ServerMessageType::COOP_GAME_LOBBY_UPDATE)
      .value("PVP_GAME_LOBBY_UPDATE",
             shared::ServerMessageType::PVP_GAME_LOBBY_UPDATE)
      .value("PVP_GAME_STATE", shared::ServerMessageType::PVP_GAME_STATE)
      .value("COOP_GAME_STATE", shared::ServerMessageType::COOP_GAME_STATE);

  emscripten::enum_<shared::ClientMessageType>(
      "ClientMessageType", emscripten::enum_value_type::number)
      .value("PLAYER_INPUT", shared::ClientMessageType::PLAYER_INPUT)
      .value("READY", shared::ClientMessageType::READY);

  emscripten::enum_<shared::Button>("Button",
                                    emscripten::enum_value_type::number)
      .value("BUTTON_NONE", shared::Button::BUTTON_NONE)
      .value("BUTTON_LEFT", shared::Button::BUTTON_LEFT)
      .value("BUTTON_RIGHT", shared::Button::BUTTON_RIGHT)
      .value("BUTTON_SHOOT", shared::Button::BUTTON_SHOOT);

  emscripten::class_<WebSim>("WebSim")
      .constructor<>()
      .function("startCoop", &WebSim::startCoop)
      .function("startPvP", &WebSim::startPvP)
      .function("step", &WebSim::step)
      .function("getPlayerIds", &WebSim::getPlayerIds);

  emscripten::class_<shared::Vec2D>("Vec2D")
      .constructor<float, float>()
      .property("x", &shared::Vec2D::x)
      .property("y", &shared::Vec2D::y)
      .function("rotated", &shared::Vec2D::rotated);
}
