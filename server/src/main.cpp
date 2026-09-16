#include "App.h"
#include "Loop.h"
#include "game.h"
#include "internal/eventing/epoll_kqueue.h"
#include "libusockets.h"
#include "shared/constants.h"
#include "shared/helpers.h"
#include "shared/messages.h"
#include "shared/rnd_generator.h"
#include <cstdio>
#include <cstring>

/// Builds and broadcasts a `LOBBY_UPDATE` to every player in @p data's
/// game: current roster (name + ready state per slot) and whether the
/// match has started. Called after any lobby-relevant change - a join, a
/// disconnect, or a ready toggle - so every client's lobby screen stays in
/// sync.
auto sendLobbyUpdate(PerSocketData *data) {
  nlohmann::json envelope;

  std::visit(
      shared::overloaded{
          [&data, &envelope](const CoopGameType &t) {
            const auto &players = data->game->getPlayers();
            shared::OptionalTypeInPlayerSlots<shared::PlayerInfo> playerInfos{};

            for (std::size_t i = 0; i < players.size(); ++i) {
              const auto player = players[i];
              if (player) {
                playerInfos[i].emplace();
                playerInfos[i] =
                    shared::PlayerInfo{.id = player->id,
                                       .name = player->name,
                                       .is_ready = player->is_ready};
              }
            }

            envelope["type"] =
                shared::ServerMessageType::COOP_GAME_LOBBY_UPDATE;
            envelope["payload"] = shared::CoopGameLobbyUpdate{
                .players = playerInfos,
                .max_players = shared::MAX_PLAYERS,
                .game_started = data->game->canStart()};
          },
          [&data, &envelope](const PvPGameType &t) {
            if (PvPGame *game = dynamic_cast<PvPGame *>(data->game)) {
              const auto &teams = game->getTeams();
              shared::OptionalTypeInTeamSlots<shared::PlayerInfo> teamsInfo;

              for (const auto &team : teams) {
                auto &teamInfo = teamsInfo[&team - &teams[0]];

                for (const auto &player : team) {
                  if (player) {
                    auto &playerInfo = teamInfo[&player - &team[0]];
                    playerInfo.emplace();
                    playerInfo =
                        shared::PlayerInfo{.id = player->id,
                                           .name = player->name,
                                           .is_ready = player->is_ready};
                  }
                }
              }

              envelope["type"] =
                  shared::ServerMessageType::PVP_GAME_LOBBY_UPDATE;
              envelope["payload"] = shared::PvPGameLobbyUpdate{
                  .teams = teamsInfo,
                  .team_size = static_cast<uint8_t>(t.team_size),
                  .game_started = data->game->canStart()};
            }
          }},
      data->game_type);

  for (const auto &player : data->game->getPlayers()) {
    if (player)
      player->ws->send(envelope.dump(), uWS::OpCode::TEXT);
  }
}

int main(int argc, char *argv[]) {
  RndGenerator::seed();

  GameManager manager{};
  auto manager_ptr = &manager;

  // Drives every active Game's simulation at a fixed 16ms tick (~60Hz),
  // independent of how many/few WebSocket messages arrive - the sim must
  // advance on a wall-clock schedule, not a message-driven one.
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
          {// The one WebSocket callback that still sees the raw HTTP
           // upgrade request - used to read the `?name=` query parameter
           // before the socket exists, since `.open` only gets the
           // WebSocket itself with no access to the original request.
           .upgrade =
               [](auto *res, auto *req, auto *context) {
                 PerSocketData data{};

                 auto playersCountStr = req->getQuery("players");

                 if (playersCountStr.empty()) {
                   data.player = new PlayerConnection{};
                   data.game_type = CoopGameType{};
                 } else {
                   auto playersCount = shared::toInt(playersCountStr);

                   if (playersCount && playersCount > 0 &&
                       playersCount <= shared::MAX_PLAYERS / 2) {
                     data.player = new PlayerConnection{};
                     data.game_type = PvPGameType{
                         .team_size = static_cast<uint8_t>(*playersCount)};
                   } else
                     return;
                 }

                 auto reqName = req->getQuery("name");

                 std::memcpy(data.player->name, reqName.data(),
                             std::min(reqName.size(), shared::MAX_NAME_LENGTH));

                 res->template upgrade<PerSocketData>(
                     std::move(data), req->getHeader("sec-websocket-key"),
                     req->getHeader("sec-websocket-protocol"),
                     req->getHeader("sec-websocket-extensions"), context);
               },
           // Connection established: assigns the real player id (the
           // server never trusts a client-supplied id), joins/creates a
           // game, and sends the WELCOME + initial LOBBY_UPDATE.
           .open =
               [&manager](auto *ws) {
                 auto *data = ws->getUserData();
                 data->player->id = manager.next_player_id++;
                 data->player->ws = ws;

                 std::visit(shared::overloaded{
                                [&data, &manager](CoopGameType) {
                                  data->game = manager.joinOrCreateCoopGame(
                                      data->player);
                                },
                                [&data, &manager](PvPGameType pvp) {
                                  data->game = manager.joinOrCreatePvPGame(
                                      data->player, pvp.team_size);
                                }},
                            data->game_type);

                 nlohmann::json welcomeEnvelope;
                 welcomeEnvelope["type"] = shared::ServerMessageType::WELCOME;
                 welcomeEnvelope["payload"] =
                     shared::WelcomeMessage{.player_id = data->player->id};
                 ws->send(welcomeEnvelope.dump());

                 sendLobbyUpdate(data);
               },
           // Dispatches an incoming `{type, payload}` envelope by
           // `ClientMessageType`. The whole parse+dispatch is wrapped in
           // try/catch: a malformed or mistyped payload just gets dropped
           // for this connection rather than crashing the process and
           // every other in-progress match.
           .message =
               [](auto *ws, std::string_view message, uWS::OpCode opCode) {
                 auto *data = ws->getUserData();

                 try {
                   auto j = nlohmann::json::parse(message);

                   switch (j.at("type").get<shared::ClientMessageType>()) {
                   case shared::ClientMessageType::READY:
                     data->game->setPlayerReady(
                         data->player->id,
                         j.at("payload").get<shared::ReadyMessage>().is_ready);
                     sendLobbyUpdate(data);
                     break;
                   case shared::ClientMessageType::PLAYER_INPUT:
                     auto input = j.at("payload").get<shared::PlayerInput>();
                     bool shoot_now =
                         input.buttons & shared::Button::BUTTON_SHOOT;
                     if (shoot_now && !data->player->prev_shoot_held)
                       data->player->pending_shots++;

                     data->player->prev_shoot_held = shoot_now;
                     data->player->pending_movement =
                         static_cast<shared::Button>(
                             input.buttons &
                             (shared::BUTTON_LEFT | shared::BUTTON_RIGHT));
                     break;
                   }
                 } catch (const nlohmann::json::exception &e) {
                   return;
                 }
               },
           // Connection closed: frees this player's slot, and either tears
           // down the whole game (if it was the last player) or lets the
           // remaining player(s) know via a fresh LOBBY_UPDATE.
           .close =
               [&manager](auto *ws, int code, std::string_view reason) {
                 auto *data = ws->getUserData();
                 data->game->removePlayer(data->player->id);

                 auto playerCount = data->game->getPlayersCount();

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
