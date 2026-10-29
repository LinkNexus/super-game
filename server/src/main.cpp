#include "App.h"
#include "Loop.h"
#include "constants.h"
#include "game.h"
#include "game_manager.h"
#include "internal/eventing/epoll_kqueue.h"
#include "libusockets.h"
#include "shared/aliases.h"
#include "shared/constants.h"
#include "shared/helpers.h"
#include "shared/messages.h"
#include "shared/rnd_generator.h"
#include "shared/wire.h"
#include "types.h"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

struct PerSocketData;

struct UwsConnection final : WsConnection {
  uWS::WebSocket<false, true, PerSocketData> *ws{};
  void send(std::string_view payload) override {
    ws->send(payload,
             shared::WIRE_BINARY ? uWS::OpCode::BINARY : uWS::OpCode::TEXT);
  }
};

struct PerSocketData {
  GameType game_type{};
  GameId game_id{};
  PerSocketPlayers players_data{};
  UwsConnection connection{};
  bool logged_decode_error{false};
  uint32_t dropped_frames{0};
};

int main(int, char *[]) {
  shared::RndGenerator::seed();

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
      .get("/version",
           [](auto *res, auto *) {
             auto version = std::getenv("SUPERGAME_VERSION");

             version ? res->end(version) : res->end("unknown");
           })
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

                 auto initPlayers = [slotsCount,
                                     players = &data.players_data.players,
                                     &req]() {
                   std::array<std::string, shared::MAX_PLAYERS_PER_CLIENT>
                       names;

                   const auto slots = static_cast<std::size_t>(*slotsCount);

                   for (std::size_t i{0}; i < slots; ++i) {
                     auto reqName =
                         req->getQuery("name" + std::to_string(i + 1));

                     if (!shared::isNameValid(reqName))
                       return false;

                     names[i] = std::string(reqName);
                   }

                   for (std::size_t i{0}; i < slots; ++i) {
                     auto &[_, name] = (*players)[i].emplace();
                     name = names[i];
                   }

                   return true;
                 };

                 if (!teamSize && slotsCountIsValid) {
                   if (!initPlayers())
                     goto bad_request;
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
                     if (!initPlayers())
                       goto bad_request;
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

                 for (auto &kvp : data->players_data.players) {
                   if (kvp) {
                     kvp->first = manager.next_player_id++;
                   }
                 }

                 Game *game{nullptr};
                 std::visit(
                     shared::overloaded{
                         [&data, &manager, &game](CoopGameType &) {
                           game =
                               manager.joinOrCreateCoopGame(data->players_data);
                         },
                         [&data, &manager, &game](PvPGameType &t) {
                           game = manager.joinOrCreatePvPGame(
                               data->players_data, t.team_size, t.team_count);
                         }},
                     data->game_type);

                 nlohmann::json welcomeEnvelope;
                 welcomeEnvelope["type"] = shared::ServerMessageType::WELCOME;

                 shared::PlayerIds<shared::MAX_PLAYERS_PER_CLIENT> ids{};
                 std::transform(data->players_data.players.begin(),
                                data->players_data.players.end(), ids.begin(),
                                [](const auto &player) {
                                  return player ? std::optional(player->first)
                                                : std::nullopt;
                                });

                 welcomeEnvelope["payload"] =
                     shared::WelcomeMessage{.players_ids = ids};
                 ws->send(shared::encode(welcomeEnvelope));

                 if (game) {
                   data->game_id = game->id;
                   game->broadcastLobbyUpdate();
                 }
               },
           // Dispatches an incoming `{type, payload}` envelope by
           // `ClientMessageType`. The whole parse+dispatch is wrapped in
           // try/catch: a malformed or mistyped payload just gets dropped
           // for this connection rather than crashing the process and
           // every other in-progress match.
           .message =
               [&manager](auto *ws, std::string_view message, uWS::OpCode) {
                 auto data = ws->getUserData();
                 auto game = manager.findGameById(data->game_id);

                 if (!game)
                   return;

                 const auto outcome =
                     game->handleClientMessage(data->players_data, message);

                 if (outcome)
                   return;

                 data->dropped_frames++;
                 if (data->logged_decode_error)
                   return;

                 data->logged_decode_error = true;
                 std::cerr << "[game " << data->game_id
                           << "] dropped client frame: " << outcome.detail
                           << "\n frame: "
                           << message.substr(
                                  0, std::min<std::size_t>(
                                         message.size(), MAX_LOGGED_FRAME_SIZE))
                           << '\n';
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
                   game->broadcastLobbyUpdate();

                 if (data->dropped_frames) {
                   std::cerr << "[game " << data->game_id << "] closed after "
                             << data->dropped_frames << " dropped frame(s)\n";
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
