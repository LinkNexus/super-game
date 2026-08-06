#include "App.h"
#include "Loop.h"
#include "game.h"
#include "internal/eventing/epoll_kqueue.h"
#include "libusockets.h"
#include "shared/constants.h"
#include "shared/messages.h"
#include <chrono>
#include <cstdio>
#include <cstring>

auto sendLobbyUpdate(PerSocketData *data) {
  auto playerCount = data->game->getPlayerCount();
  shared::LobbyUpdate lobbyUpdate{
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
        static int ticks = 0;
        static auto last = std::chrono::steady_clock::now();
        if (++ticks % 60 == 0) {
          auto now = std::chrono::steady_clock::now();
          std::printf(
              "60 timer firings in %lldms\n",
              (long long)std::chrono::duration_cast<std::chrono::milliseconds>(
                  now - last)
                  .count());
          last = now;
        }

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
      .ws<PerSocketData>(
          "/*",
          {.open =
               [&manager](auto *ws) {
                 auto *data = ws->getUserData();
                 auto *player =
                     new PlayerConnection{manager.next_player_id++, ws};
                 data->player = player;
                 data->game = manager.joinOrCreateGame(player);

                 nlohmann::json welcomeEnvelope;
                 welcomeEnvelope["type"] = shared::MessageType::WELCOME;
                 welcomeEnvelope["payload"] =
                     shared::WelcomeMessage{.player_id = player->id};
                 ws->send(welcomeEnvelope.dump());

                 sendLobbyUpdate(data);
               },
           .message =
               [](auto *ws, std::string_view message, uWS::OpCode opCode) {
                 auto *data = ws->getUserData();
                 auto input =
                     nlohmann::json::parse(message).get<shared::PlayerInput>();

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
      .listen(9001,
              [](auto *token) {
                if (token) {
                  std::printf("uWebSockets listening on port 9001\n");
                } else {
                  std::printf("uWebSockets failed to listen on port 9001\n");
                }
              })
      .run();

  return 0;
}
