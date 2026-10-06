#include "App.h"
#include "Loop.h"
#include "game.h"
#include "game_manager.h"
#include "internal/eventing/epoll_kqueue.h"
#include "libusockets.h"
#include "shared/aliases.h"
#include "shared/constants.h"
#include "shared/helpers.h"
#include "shared/messages.h"
#include "shared/rnd_generator.h"
#include "types.h"
#include <cstdio>
#include <cstring>

struct PerSocketData;

struct UwsConnection final : WsConnection {
  uWS::WebSocket<false, true, PerSocketData> *ws{};
  void send(std::string_view payload) override {
    ws->send(payload, uWS::OpCode::TEXT);
  }
};

struct PerSocketData {
  GameType game_type{};
  GameId game_id{};
  PerSocketPlayers players_data{};
  UwsConnection connection{};
};

/// Builds and broadcasts a `LOBBY_UPDATE` to every player in @p data's
/// game: current roster (name + ready state per slot) and whether the
/// match has started. Called after any lobby-relevant change - a join, a
/// disconnect, or a ready toggle - so every client's lobby screen stays in
/// sync.
auto sendLobbyUpdate(const GameManager &manager, PerSocketData *data) {
  nlohmann::json envelope;
  auto g = manager.findGameById(data->game_id);
  if (!g)
    return;

  std::visit(
      shared::overloaded{
          [&g, &envelope](const CoopGameType &) {
            auto game = dynamic_cast<CoopGame *>(g);
            const auto &players = game->getPlayers();

            std::array<shared::PlayerInfo, shared::MAX_PLAYERS_COOP>
                playersInfos{};
            shared::PlayerCount idx{0};
            for (const auto &player : players) {
              if (player) {
                playersInfos[idx++] =
                    shared::PlayerInfo{.id = player->id,
                                       .name = player->name,
                                       .is_ready = player->is_ready};
              }
            }

            envelope["type"] =
                shared::ServerMessageType::COOP_GAME_LOBBY_UPDATE;
            envelope["payload"] = shared::CoopGameLobbyUpdate{
                .players = playersInfos,
                .players_count = shared::count_optional(players),
                .max_players_count = shared::MAX_PLAYERS_COOP,
                .game_started = game->isRunning()};
          },
          [&g, &envelope](const PvPGameType &t) {
            auto game = dynamic_cast<PvPGame *>(g);
            const auto &teams = game->getTeams();

            std::array<shared::PvPGameLobbyUpdate::Team, shared::MAX_TEAMS>
                teamsInfos{};

            for (std::size_t teamIndex = 0; teamIndex < teams.size();
                 ++teamIndex) {
              const auto &team = teams[teamIndex];

              shared::PlayerCount idx{0};
              for (const auto &player : team.players) {
                if (player)
                  teamsInfos[teamIndex].players[idx++] =
                      shared::PlayerInfo{.id = player->id,
                                         .name = player->name,
                                         .is_ready = player->is_ready};
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

  for (const auto &socket : g->getPlayerSockets()) {
    if (socket)
      socket->send(envelope.dump());
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
          manager->destroyGame(g->id);
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

                 auto teamSize = shared::toInt(req->getQuery("team_size"));
                 auto slotsCount = shared::toInt(req->getQuery("slots"));
                 auto slotsCountIsValid =
                     slotsCount && *slotsCount > 0 &&
                     *slotsCount <= shared::MAX_PLAYERS_PER_CLIENT;

                 if (!teamSize && slotsCountIsValid) {
                   data.players_data.count =
                       static_cast<shared::PlayerCount>(*slotsCount);
                   data.game_type = CoopGameType{};
                 } else if (teamSize && slotsCountIsValid) {
                   auto teamCount = shared::toInt(req->getQuery("team_count"));
                   auto teamCountIsValid = teamCount && *teamCount > 0 &&
                                           *teamCount <= shared::MAX_TEAMS;
                   auto teamSizeIsValid =
                       *teamSize > 0 &&
                       *teamSize <= shared::MAX_PLAYERS_PER_TEAM;

                   if (teamSizeIsValid && teamCountIsValid &&
                       *slotsCount <= *teamSize) {
                     data.players_data.count =
                         static_cast<shared::PlayerCount>(*slotsCount);
                     data.game_type = PvPGameType{
                         .team_size =
                             static_cast<shared::PlayerCount>(*teamSize),
                         .team_count =
                             static_cast<shared::PlayerCount>(*teamCount)};
                   } else
                     goto bad_request;

                 } else {
                   goto bad_request;
                 }

                 for (std::size_t i = 0; i < data.players_data.count; ++i) {
                   auto reqName = req->getQuery("name" + std::to_string(i + 1));
                   data.players_data.names[i] = std::string(reqName);
                 }

                 res->template upgrade<PerSocketData>(
                     std::move(data), req->getHeader("sec-websocket-key"),
                     req->getHeader("sec-websocket-protocol"),
                     req->getHeader("sec-websocket-extensions"), context);
                 return;

               bad_request:
                 res->writeStatus("400 Bad Request")->end();
               },
           // Connection established: assigns the real player id (the
           // server never trusts a client-supplied id), joins/creates a
           // game, and sends the WELCOME + initial LOBBY_UPDATE.
           .open =
               [&manager](auto *ws) {
                 auto *data = ws->getUserData();

                 data->connection.ws = ws;
                 data->players_data.ws = &data->connection;

                 for (int i = 0; i < data->players_data.count; ++i) {
                   data->players_data.players[i] = manager.next_player_id++;
                 }

                 std::visit(shared::overloaded{
                                [&data, &manager](CoopGameType &) {
                                  if (auto res = manager.joinOrCreateCoopGame(
                                          data->players_data))
                                    data->game_id = res.value();
                                },
                                [&data, &manager](PvPGameType &t) {
                                  if (auto res = manager.joinOrCreatePvPGame(
                                          data->players_data, t.team_size,
                                          t.team_count))
                                    data->game_id = res.value();
                                }},
                            data->game_type);

                 nlohmann::json welcomeEnvelope;
                 welcomeEnvelope["type"] = shared::ServerMessageType::WELCOME;

                 welcomeEnvelope["payload"] = shared::WelcomeMessage{
                     .players_ids = data->players_data.players};
                 ws->send(welcomeEnvelope.dump());

                 sendLobbyUpdate(manager, data);
               },
           // Dispatches an incoming `{type, payload}` envelope by
           // `ClientMessageType`. The whole parse+dispatch is wrapped in
           // try/catch: a malformed or mistyped payload just gets dropped
           // for this connection rather than crashing the process and
           // every other in-progress match.
           .message =
               [&manager](auto *ws, std::string_view message, uWS::OpCode) {
                 auto *data = ws->getUserData();

                 try {
                   auto j = nlohmann::json::parse(message);

                   switch (j.at("type").get<shared::ClientMessageType>()) {
                   case shared::ClientMessageType::READY: {
                     auto game = manager.findGameById(data->game_id);
                     if (game) {
                       game->setPlayersReady(data->players_data.ws,
                                             j.at("payload")
                                                 .get<shared::ReadyMessage>()
                                                 .is_ready);
                     }
                     sendLobbyUpdate(manager, data);
                     break;
                   }

                   case shared::ClientMessageType::PLAYER_INPUT:
                     auto game = manager.findGameById(data->game_id);
                     if (!game)
                       return;

                     auto inputs =
                         j.at("payload")
                             .get<std::array<shared::PlayerInput,
                                             shared::MAX_PLAYERS_PER_CLIENT>>();

                     for (std::size_t i = 0; i < data->players_data.count;
                          ++i) {
                       const auto &input = inputs[i];

                       auto endIt = data->players_data.players.begin() +
                                    data->players_data.count;
                       auto playerIt =
                           std::find(data->players_data.players.begin(), endIt,
                                     input.player_id);

                       if (playerIt == endIt)
                         continue;

                       auto player = game->findPlayerById(*playerIt);
                       if (!player)
                         continue;

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
                 } catch (const nlohmann::json::exception &) {
                   return;
                 }
               },
           // Connection closed: frees this player's slot, and either tears
           // down the whole game (if it was the last player) or lets the
           // remaining player(s) know via a fresh LOBBY_UPDATE.
           .close =
               [&manager](auto *ws, int, std::string_view) {
                 auto *data = ws->getUserData();
                 auto game = manager.findGameById(data->game_id);
                 if (!game)
                   return;

                 game->removePlayers(data->players_data.ws);

                 if (game->isEmpty()) {
                   manager.destroyGame(data->game_id);
                   return;
                 }

                 if (!game->isOver() && !game->isRunning())
                   sendLobbyUpdate(manager, data);
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
