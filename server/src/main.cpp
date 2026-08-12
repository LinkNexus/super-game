#include "App.h"
#include "Loop.h"
#include "game.h"
#include "internal/eventing/epoll_kqueue.h"
#include "libusockets.h"
#include "shared/constants.h"
#include "shared/messages.h"
#include <cstdio>
#include <cstring>

auto sendLobbyUpdate(PerSocketData *data) {
  auto playerCount = data->game->getPlayerCount();
  const auto &players = data->game->getPlayers();
  std::array<std::optional<shared::PlayerInfo>, shared::MAX_PLAYERS>
      playerInfos{};

  for (std::size_t i = 0; i < players.size(); ++i) {
    const auto player = players[i];
    if (player) {
      playerInfos[i].emplace();
      playerInfos[i] = shared::PlayerInfo{.name = player->name};
    }
  }

  shared::LobbyUpdate lobbyUpdate{
      .players = playerInfos,
      .player_count = static_cast<uint8_t>(playerCount),
      .game_started = playerCount == shared::MAX_PLAYERS,
      .max_players = shared::MAX_PLAYERS};

  nlohmann::json envelope;
  envelope["type"] = shared::MessageType::LOBBY_UPDATE;
  envelope["payload"] = lobbyUpdate;

  for (const auto &player : data->game->getPlayers()) {
    if (player)
      player->ws->send(envelope.dump(), uWS::OpCode::TEXT);
  }
}

int main(int argc, char *argv[]) {
  GameManager manager{};
  auto manager_ptr = &manager;

  auto *timer =
      us_create_timer((us_loop_t *)uWS::Loop::get(), 0, sizeof(GameManager *));
  memcpy(us_timer_ext(timer), &manager_ptr, sizeof(GameManager *));
  us_timer_set(
      timer,
      [](auto t) {
        GameManager *manager;
        memcpy(&manager, us_timer_ext(t), sizeof(GameManager *));

        std::vector<Game *> finishedGames;
        manager->forEachGame([&manager, &finishedGames](Game *g) {
          g->update(shared::FIXED_DT);
          if (g->isOver())
            finishedGames.push_back(g);
        });
        for (auto *g : finishedGames)
          manager->destroyGame(g);
      },
      16, 16);

  uWS::App()
      .get("/health", [](auto *res, auto *req) { res->end("OK"); })
      .ws<PerSocketData>(
          "/*",
          {.upgrade =
               [](auto *res, auto *req, auto *context) {
                 PerSocketData data{.player = new PlayerConnection{}};
                 auto reqName = req->getQuery("name");

                 std::memcpy(data.player->name, reqName.data(),
                             std::min(reqName.size(), shared::MAX_NAME_LENGTH));

                 res->template upgrade<PerSocketData>(
                     std::move(data), req->getHeader("sec-websocket-key"),
                     req->getHeader("sec-websocket-protocol"),
                     req->getHeader("sec-websocket-extensions"), context);
               },
           .open =
               [&manager](auto *ws) {
                 auto *data = ws->getUserData();
                 data->player->id = manager.next_player_id++;
                 data->player->ws = ws;
                 data->game = manager.joinOrCreateGame(data->player);

                 nlohmann::json welcomeEnvelope;
                 welcomeEnvelope["type"] = shared::MessageType::WELCOME;
                 welcomeEnvelope["payload"] =
                     shared::WelcomeMessage{.player_id = data->player->id};
                 ws->send(welcomeEnvelope.dump());

                 sendLobbyUpdate(data);
               },
           .message =
               [](auto *ws, std::string_view message, uWS::OpCode opCode) {
                 auto *data = ws->getUserData();
                 shared::PlayerInput input{};

                 try {
                   input = nlohmann::json::parse(message)
                               .get<shared::PlayerInput>();
                 } catch (const nlohmann::json::exception &e) {
                   return;
                 }

                 bool shoot_now = input.buttons & shared::Button::BUTTON_SHOOT;
                 if (shoot_now && !data->player->prev_shoot_held)
                   data->player->pending_shots++;

                 data->player->prev_shoot_held = shoot_now;
                 data->player->pending_movement = static_cast<shared::Button>(
                     input.buttons &
                     (shared::BUTTON_LEFT | shared::BUTTON_RIGHT));
               },
           .close =
               [&manager](auto *ws, int code, std::string_view reason) {
                 auto *data = ws->getUserData();
                 data->game->removePlayer(data->player->id);

                 auto playerCount = data->game->getPlayerCount();

                 if (playerCount == 0)
                   manager.destroyGame(data->game);
                 else
                   sendLobbyUpdate(data);
               }})
      .listen("0.0.0.0", 9001,
              [](auto *token) {
                setvbuf(stdout, nullptr, _IONBF, 0);
                if (token) {
                  std::printf("uWebSockets listening on port 9001\n");
                } else {
                  std::printf("uWebSockets failed to listen on port 9001\n");
                }
                std::fflush(stdout);
              })
      .run();

  return 0;
}
