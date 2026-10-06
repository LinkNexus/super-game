#pragma once

#include "game.h"
#include "shared/helpers.h"
#include "types.h"
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

/// Matchmaking: assigns newly-connected players to an open game (creating
/// one if needed) and owns every `Game`'s lifetime.
class GameManager {
public:
  ServerPlayerId next_player_id{1};
  GameId next_game_id{1};

public:
  std::optional<GameId>
  joinOrCreateCoopGame(const PerSocketPlayers &players_data);

  CoopGame *createCoopGame();

  std::optional<GameId>
  joinOrCreatePvPGame(const PerSocketPlayers &players_data,
                      shared::PlayerCount team_size,
                      shared::PlayerCount team_count);

  PvPGame *createPvPGame(shared::PlayerCount team_size,
                         shared::PlayerCount team_count);

  Game *findGameById(GameId id) const;
  void destroyGame(GameId id);
  void forEachGame(std::function<void(Game *)> fn);

private:
  std::unordered_map<GameId, std::unique_ptr<Game>> games_by_id{};
  std::vector<CoopGame *> open_coop_games_{};
  std::unordered_map<std::pair<shared::PlayerCount, shared::PlayerCount>,
                     std::vector<PvPGame *>, shared::PairHash>
      open_pvp_games_{};
};
