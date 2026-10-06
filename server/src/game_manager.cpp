#include "game_manager.h"
#include "game.h"

void GameManager::forEachGame(std::function<void(Game *)> fn) {
  for (const auto &game : games_by_id) {
    fn(game.second.get());
  }
}

CoopGame *GameManager::createCoopGame() {
  auto game = std::make_unique<CoopGame>();
  game->id = next_game_id++;
  auto gamePtr = game.get();
  games_by_id[game->id] = std::move(game);
  return gamePtr;
}

std::optional<GameId>
GameManager::joinOrCreateCoopGame(const PerSocketPlayers &players_data) {
  for (auto game : open_coop_games_) {
    if (game->addPlayers(players_data)) {
      return game->id;
    }
  }

  auto new_game = createCoopGame();
  open_coop_games_.push_back(new_game);
  return new_game->addPlayers(players_data)
             ? std::optional<GameId>{new_game->id}
             : std::nullopt;
}

PvPGame *GameManager::createPvPGame(shared::PlayerCount team_size,
                                    shared::PlayerCount team_count) {
  auto game = std::make_unique<PvPGame>(team_size, team_count);
  game->id = next_game_id++;
  auto gamePtr = game.get();
  games_by_id[game->id] = std::move(game);
  return gamePtr;
}

std::optional<GameId>
GameManager::joinOrCreatePvPGame(const PerSocketPlayers &players_data,
                                 shared::PlayerCount team_size,
                                 shared::PlayerCount team_count) {
  auto &openGames = open_pvp_games_[{team_count, team_size}];

  for (auto game : openGames) {
    if (game->addPlayers(players_data)) {
      return game->id;
    }
  }

  auto new_game = createPvPGame(team_size, team_count);
  openGames.push_back(new_game);
  return new_game->addPlayers(players_data)
             ? std::optional<GameId>{new_game->id}
             : std::nullopt;
}

Game *GameManager::findGameById(GameId id) const {
  auto it = games_by_id.find(id);
  if (it != games_by_id.end()) {
    return it->second.get();
  }
  return nullptr;
}

void GameManager::destroyGame(GameId gameId) {
  auto game = findGameById(gameId);
  if (!game)
    return;

  open_coop_games_.erase(
      std::remove(open_coop_games_.begin(), open_coop_games_.end(), game),
      open_coop_games_.end());

  for (auto &kvp : open_pvp_games_) {
    auto it = std::find(kvp.second.begin(), kvp.second.end(), game);

    if (it != kvp.second.end()) {
      kvp.second.erase(std::remove(kvp.second.begin(), kvp.second.end(), game),
                       kvp.second.end());
      break;
    }
  }

  games_by_id.erase(gameId);
}
