#include "App.h"
#include "game.h"
#include "shared/constants.h"
#include "shared/messages.h"
#include <cstdio>

int main(int argc, char *argv[]) {
  GameManager manager;

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

                 auto playerCount = data->game->getPlayerCount();
                 shared::LobbyUpdate lobbyUpdate{
                     .player_count = static_cast<uint8_t>(playerCount),
                     .game_started = playerCount == MAX_PLAYERS};

                 nlohmann::json envelope;
                 envelope["type"] = shared::MessageType::LOBBY_UPDATE;
                 envelope["payload"] = lobbyUpdate;

                 if (data->game->getPlayerCount() == MAX_PLAYERS) {
                   for (const auto &player : data->game->getPlayers()) {
                     player->ws->send(envelope.dump(), uWS::OpCode::TEXT);
                   }
                 }
               },
           .message = [](auto *ws, std::string_view message,
                         uWS::OpCode opCode) { ws->send(message, opCode); },
           .close =
               [&manager](auto *ws, int code, std::string_view reason) {
                 auto *data = ws->getUserData();
                 data->game->removePlayer(data->player->id);
                 if (data->game->getPlayerCount() == 0)
                   manager.destroyGame(data->game);
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
