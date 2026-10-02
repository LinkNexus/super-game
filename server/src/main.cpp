#include "App.h"
#include "Loop.h"
#include "game.h"
#include "internal/eventing/epoll_kqueue.h"
#include "libusockets.h"
#include "shared/aliases.h"
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
          [&data, &envelope](const CoopGameType &) {
            auto game = dynamic_cast<CoopGame *>(data->game);
            const auto &players = game->getPlayers();

            std::array<shared::PlayerInfo, shared::MAX_PLAYERS_COOP>
                playersInfos{};

            for (std::size_t i = 0; i < game->getPlayersCount(); ++i) {
              const auto player = players[i];
              playersInfos[i] =
                  shared::PlayerInfo{.id = player->id,
                                     .name = player->name,
                                     .is_ready = player->is_ready};
            }

            envelope["type"] =
                shared::ServerMessageType::COOP_GAME_LOBBY_UPDATE;
            envelope["payload"] = shared::CoopGameLobbyUpdate{
                .players = playersInfos,
                .players_count = game->getPlayersCount(),
                .max_players_count = shared::MAX_PLAYERS_COOP,
                .game_started = game->isRunning()};
          },
          [&data, &envelope](const PvPGameType &t) {
            auto game = dynamic_cast<PvPGame *>(data->game);
            const auto &teams = game->getTeams();

            std::array<shared::PvPGameLobbyUpdate::Team, shared::MAX_TEAMS>
                teamsInfos{};

            for (std::size_t teamIndex = 0; teamIndex < teams.size();
                 ++teamIndex) {
              const auto &team = teams[teamIndex];
              teamsInfos[teamIndex].players_count = team.players_count;

              for (std::size_t playerIndex = 0;
                   playerIndex < team.players_count; ++playerIndex) {
                teamsInfos[teamIndex].players[playerIndex] = shared::PlayerInfo{
                    .id = team.players[playerIndex]->id,
                    .name = team.players[playerIndex]->name,
                    .is_ready = team.players[playerIndex]->is_ready};
              }
            }

            envelope["type"] = shared::ServerMessageType::PVP_GAME_LOBBY_UPDATE;
            envelope["payload"] =
                shared::PvPGameLobbyUpdate{.teams = teamsInfos,
                                           .team_count = t.team_count,
                                           .team_size = t.team_size,
                                           .game_started = game->isRunning()};
          }},
      data->game_type);

  for (const auto &socket : data->game->getPlayerSockets()) {
    if (socket)
      socket->send(envelope.dump(), uWS::OpCode::TEXT);
  }
}

int main(int, char *[]) {
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
        manager->forEachGame([&finishedGames](Game *g) {
          g->update(shared::FIXED_DT);
          if (g->isOver())
            finishedGames.push_back(g);
        });
        for (auto *g : finishedGames)
          manager->destroyGame(g);
      },
      16, 16);

  uWS::App()
      .get("/health", [](auto *res, auto *) { res->end("OK"); })
      .ws<PerSocketData>(
          "/*",
          {// The one WebSocket callback that still sees the raw HTTP
           // upgrade request - used to read the `?name=` query parameter
           // before the socket exists, since `.open` only gets the
           // WebSocket itself with no access to the original request.
           .upgrade =
               [](auto *res, auto *req, auto *context) {
                 PerSocketData data{};

                 auto teamSizeStr = req->getQuery("team_size");

                 auto slotsCountStr = req->getQuery("slots");
                 auto slotsCount =
                     slotsCountStr.empty() ? 1 : shared::toInt(slotsCountStr);

                 if (teamSizeStr.empty()) {
                   if (slotsCount &&
                       slotsCount <= shared::MAX_PLAYERS_PER_CLIENT) {
                     for (std::size_t i = 0; i < slotsCount; ++i) {
                       data.players_data.players[i] = new PlayerConnection{};
                     }
                     data.players_data.count =
                         static_cast<uint8_t>(*slotsCount);
                     data.game_type = CoopGameType{};
                   } else
                     goto upgrade;
                 } else {
                   auto teamSize = shared::toInt(teamSizeStr);
                   auto teamCount = shared::toInt(req->getQuery("team_count"));

                   if (teamSize && teamSize > 0 &&
                       teamSize <= shared::MAX_PLAYERS_PER_TEAM && slotsCount &&
                       slotsCount <= shared::MAX_PLAYERS_PER_CLIENT &&
                       slotsCount <= teamSize && teamCount && teamCount > 0 &&
                       teamCount <= shared::MAX_TEAMS) {
                     for (std::size_t i = 0; i < slotsCount; ++i) {
                       data.players_data.players[i] = new PlayerConnection{};
                     }
                     data.players_data.count =
                         static_cast<uint8_t>(*slotsCount);
                     data.game_type = PvPGameType{
                         .team_size =
                             static_cast<shared::PlayerCount>(*teamSize),
                         .team_count =
                             static_cast<shared::PlayerCount>(*teamCount)};
                   } else
                     goto upgrade;
                 }

                 for (std::size_t i = 0; i < data.players_data.players.size();
                      ++i) {
                   auto player = data.players_data.players[i];

                   if (player) {
                     auto reqName =
                         req->getQuery("name" + std::to_string(i + 1));
                     std::memcpy(
                         data.players_data.players[i]->name, reqName.data(),
                         std::min(reqName.size(), shared::MAX_NAME_LENGTH));
                   }
                 }

               upgrade:
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

                 data->players_data.ws = ws;
                 for (auto player : data->players_data.players) {
                   if (player) {
                     player->ws = ws;
                     player->id = manager.next_player_id++;
                   }
                 }

                 std::visit(shared::overloaded{
                                [&data, &manager](CoopGameType) {
                                  data->game = manager.joinOrCreateCoopGame(
                                      data->players_data);
                                },
                                [&data, &manager](PvPGameType pvp) {
                                  data->game = manager.joinOrCreatePvPGame(
                                      data->players_data, pvp.team_size,
                                      pvp.team_count);
                                }},
                            data->game_type);

                 nlohmann::json welcomeEnvelope;
                 welcomeEnvelope["type"] = shared::ServerMessageType::WELCOME;

                 std::array<shared::PlayerId, shared::MAX_PLAYERS_PER_CLIENT>
                     playerIds{};
                 for (std::size_t i = 0; i < data->players_data.count; ++i) {
                   playerIds[i] = data->players_data.players[i]->id;
                 }

                 welcomeEnvelope["payload"] =
                     shared::WelcomeMessage{.players_ids = playerIds};
                 ws->send(welcomeEnvelope.dump());

                 sendLobbyUpdate(data);
               },
           // Dispatches an incoming `{type, payload}` envelope by
           // `ClientMessageType`. The whole parse+dispatch is wrapped in
           // try/catch: a malformed or mistyped payload just gets dropped
           // for this connection rather than crashing the process and
           // every other in-progress match.
           .message =
               [](auto *ws, std::string_view message, uWS::OpCode) {
                 auto *data = ws->getUserData();

                 try {
                   auto j = nlohmann::json::parse(message);

                   switch (j.at("type").get<shared::ClientMessageType>()) {
                   case shared::ClientMessageType::READY:
                     data->game->setPlayersReady(
                         data->players_data.ws,
                         j.at("payload").get<shared::ReadyMessage>().is_ready);
                     sendLobbyUpdate(data);
                     break;

                   case shared::ClientMessageType::PLAYER_INPUT:
                     auto inputs =
                         j.at("payload")
                             .get<std::array<shared::PlayerInput,
                                             shared::MAX_PLAYERS_PER_CLIENT>>();

                     for (std::size_t i = 0; i < data->players_data.count;
                          ++i) {
                       const auto &input = inputs[i];
                       auto playerIt = std::find_if(
                           data->players_data.players.begin(),
                           data->players_data.players.begin() +
                               data->players_data.count,
                           [&input](const auto *player) {
                             return player && input.player_id == player->id;
                           });

                       if (playerIt == data->players_data.players.begin() +
                                           data->players_data.count)
                         continue;

                       auto *player = *playerIt;
                       bool shoot_now =
                           input.buttons & shared::Button::BUTTON_SHOOT;
                       if (shoot_now && !player->prev_shoot_held)
                         player->pending_shots++;

                       player->prev_shoot_held = shoot_now;
                       player->pending_movement = static_cast<shared::Button>(
                           input.buttons &
                           (shared::BUTTON_LEFT | shared::BUTTON_RIGHT));
                     }

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
               [](auto *ws, int, std::string_view) {
                 auto *data = ws->getUserData();

                 if (data->game) {
                   if (!data->game->isOver()) {
                     data->game->removePlayers(data->players_data.ws);

                     if (!data->game->isRunning()) {
                       sendLobbyUpdate(data);
                     }
                   }

                   for (std::size_t i = 0; i < data->players_data.count; ++i) {
                     delete data->players_data.players[i];
                   }
                 }
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
