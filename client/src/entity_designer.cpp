#include "entity_designer.h"
#include "constants.h"
#include "mathlib.hpp"
#include "raylib.h"
#include "shared/constants.h"
#include "shared/helpers.h"
#include "shared/messages.h"
#include "types.h"
#include "utils.h"
#include <algorithm>
#include <cstddef>
#include <optional>
#include <utility>

void EntityDesigner::loadTextures() {
  boss_texture_ = LoadTexture("assets/boss.png");
  player_ship_texture_ = LoadTexture("assets/playerShip.png");
  player_live_texture_ = LoadTexture("assets/heart.png");

  enemies_textures_[0] = LoadTexture("assets/enemy1.png");
  enemies_textures_[1] = LoadTexture("assets/enemy2.png");
}

void EntityDesigner::unloadTextures() {
  UnloadTexture(boss_texture_);
  UnloadTexture(player_ship_texture_);
  UnloadTexture(player_live_texture_);

  for (auto &texture : enemies_textures_) {
    UnloadTexture(texture);
  }
}

void EntityDesigner::drawBossHealthBar(const shared::BossState &state) const {
  float bar_x = (shared::SCREEN_WIDTH - BOSS_HEALTH_BAR_WIDTH) / 2.0f;
  float health_ratio =
      state.max_health > 0
          ? std::clamp((float)state.health / state.max_health, 0.0f, 1.0f)
          : 0.0f;

  DrawRectangle((int)bar_x, (int)BOSS_HEALTH_BAR_Y, (int)BOSS_HEALTH_BAR_WIDTH,
                (int)BOSS_HEALTH_BAR_HEIGHT, GRAY);
  DrawRectangle((int)bar_x, (int)BOSS_HEALTH_BAR_Y,
                (int)(BOSS_HEALTH_BAR_WIDTH * health_ratio),
                (int)BOSS_HEALTH_BAR_HEIGHT, RED);
  DrawRectangleLines((int)bar_x, (int)BOSS_HEALTH_BAR_Y,
                     (int)BOSS_HEALTH_BAR_WIDTH, (int)BOSS_HEALTH_BAR_HEIGHT,
                     WHITE);
}

void EntityDesigner::drawBoss(const shared::BossState &state,
                              bool canDrawHealthBar) const {
  if (!state.active)
    return;

  if (canDrawHealthBar)
    drawBossHealthBar(state);

  auto screenPosition = toScreen(state.position);

  if (boss_texture_.id != 0) {
    float scale = 80.0f / boss_texture_.width;
    Vector2 draw_pos = {screenPosition.x - (boss_texture_.width * scale) / 2.0f,
                        screenPosition.y -
                            (boss_texture_.height * scale) / 2.0f};
    DrawTextureEx(boss_texture_, draw_pos, 0.0f, scale, WHITE);
  } else {
    DrawRectangle((int)screenPosition.x - 40, (int)screenPosition.y + 20, 80,
                  40, RED);
  }
}

void EntityDesigner::drawEnemies(const shared::EnemiesPoolState &enemies,
                                 mathlib::Vec2f offset) const {
  for (std::size_t idx = 0; idx < enemies.size(); ++idx) {
    if (enemies[idx].first) {
      float pos_x = offset.x + (idx % shared::ENEMY_COLS) *
                                   (shared::EnemySimState::WIDTH +
                                    shared::EnemiesPoolSimState::SPACING_X);
      float pos_y = offset.y - (idx / shared::ENEMY_COLS) *
                                   (shared::EnemySimState::HEIGHT +
                                    shared::EnemiesPoolSimState::SPACING_Y);

      drawEnemy(toRaylibVec({pos_x, pos_y}), enemies[idx].second);
    }
  }
}

void EntityDesigner::drawEnemy(Vector2 pos, shared::EnemyType type) const {
  auto textureIdx = (type == shared::EnemyType::TYPE_1) ? 0 : 1;
  const auto &texture = enemies_textures_[textureIdx];

  if (texture.id != 0) {
    float scale = shared::EnemySimState::WIDTH / texture.width;
    Vector2 draw_pos = {pos.x - (texture.width * scale) / 2.0f,
                        pos.y - (texture.height * scale) / 2.0f};
    DrawTextureEx(texture, draw_pos, 0.0f, scale, WHITE);
  } else {
    Color c = (type == shared::EnemyType::TYPE_1) ? GREEN : ORANGE;
    DrawRectangle((int)pos.x - shared::EnemySimState::WIDTH / 2,
                  (int)pos.y - shared::EnemySimState::HEIGHT / 2,
                  shared::EnemySimState::WIDTH, shared::EnemySimState::HEIGHT,
                  c);
  }
}

void EntityDesigner::drawPlayerStats(const shared::PlayerState &state,
                                     float &yOffset, GameType gameType,
                                     bool isOnThisMachine) const {
  std::string playerLabel{};

  if (state.name.empty()) {
    playerLabel = "Player " + std::to_string(state.id);
  } else {
    playerLabel = state.name;
  }

  if (gameType == GameType::ONLINE && isOnThisMachine) {
    playerLabel = playerLabel + " (You)";
  }

  DrawText(playerLabel.c_str(),
           shared::SCREEN_WIDTH - 10 -
               MeasureText(playerLabel.c_str(), STATUS_FONT_SIZE),
           yOffset, STATUS_FONT_SIZE, WHITE);
  yOffset += STATUS_FONT_SIZE;

  drawLives(state, yOffset);
  yOffset += HEART_SIZE;

  const auto pointsText = TextFormat("Points: %d", state.points);
  DrawText(pointsText,
           shared::SCREEN_WIDTH - 10 -
               MeasureText(pointsText, STATUS_FONT_SIZE),
           yOffset, STATUS_FONT_SIZE, WHITE);
  yOffset += STATUS_FONT_SIZE + 5;

  DrawLine(shared::SCREEN_WIDTH - 10 -
               MeasureText(pointsText, STATUS_FONT_SIZE),
           yOffset, shared::SCREEN_WIDTH - 10, yOffset, WHITE);
  yOffset += 25;
}

void EntityDesigner::drawLives(const shared::PlayerState &state,
                               float &yOffset) const {
  if (state.lives == 0) {
    auto text = "Dead";
    DrawText(text,
             shared::SCREEN_WIDTH - 5 - MeasureText(text, STATUS_FONT_SIZE) -
                 HEART_SIZE,
             yOffset + (HEART_SIZE - STATUS_FONT_SIZE) / 2, STATUS_FONT_SIZE,
             WHITE);
    return;
  }

  auto livexText = std::to_string(state.lives);
  auto textWidth = MeasureText(livexText.c_str(), STATUS_FONT_SIZE);
  auto heartScale = HEART_SIZE / static_cast<float>(player_live_texture_.width);
  float heartX = shared::SCREEN_WIDTH - 10 - HEART_SIZE;
  float textX = heartX - 5 - textWidth;

  DrawText(livexText.c_str(), textX,
           yOffset + (HEART_SIZE - STATUS_FONT_SIZE) / 2, STATUS_FONT_SIZE,
           WHITE);

  if (player_live_texture_.id != 0) {
    DrawTextureEx(player_live_texture_, {heartX, yOffset - 3}, 0.0f, heartScale,
                  WHITE);
  } else {
    livexText = livexText + (state.lives > 1 ? " lives" : " life");
    DrawText(livexText.c_str(),
             shared::SCREEN_WIDTH - 10 -
                 MeasureText(livexText.c_str(), STATUS_FONT_SIZE),
             yOffset, STATUS_FONT_SIZE, WHITE);
  }
}

void EntityDesigner::drawPlayer(const shared::PlayerState &state, Color color,
                                int64_t playerIdx) const {
  auto positionOnScreen = toScreen(state.position);
  const auto size = shared::PlayerSimState::SIZE;

  if (player_ship_texture_.id != 0) {
    float scale = (size * 2) / player_ship_texture_.width;
    Rectangle source{0, 0, (float)player_ship_texture_.width,
                     (float)player_ship_texture_.height};
    Rectangle dest{positionOnScreen.x, positionOnScreen.y,
                   (float)player_ship_texture_.width * scale,
                   (float)player_ship_texture_.height * scale};
    Vector2 origin{dest.width / 2.0f, dest.height / 2.0f};

    DrawTexturePro(player_ship_texture_, source, dest, origin,
                   mathlib::toDegrees(state.orientation), color);
  } else {
    Vector2 tip = {positionOnScreen.x, positionOnScreen.y - size};
    Vector2 left = {positionOnScreen.x - size * 0.7f,
                    positionOnScreen.y + size * 0.7f};
    Vector2 right = {positionOnScreen.x + size * 0.7f,
                     positionOnScreen.y + size * 0.7f};

    DrawTriangle(tip, left, right, color);
    DrawTriangleLines(tip, left, right, WHITE);
  }

  if (playerIdx >= 0) {
    const float drawnHalfHeight =
        player_ship_texture_.id != 0
            ? (player_ship_texture_.height *
               ((size * 2) / player_ship_texture_.width)) /
                  2.0f
            : size * 0.7f;

    auto markerPos = toScreen(state.position +
                              mathlib::Vec2f(0, -(drawnHalfHeight + MARKER_GAP))
                                  .rotated(state.orientation));
    auto marker = std::string("P") + std::to_string(playerIdx + 1);

    DrawText(marker.c_str(),
             markerPos.x -
                 (MeasureText(marker.c_str(), MARKER_FONT_SIZE) / 2.0f),
             markerPos.y, MARKER_FONT_SIZE, WHITE);
  }
}

void EntityDesigner::drawPlayersForCoopGame(const shared::CoopGameState &state,
                                            Session *session,
                                            GameType gameType) const {
  auto yOffset = 10.0f;
  const auto playerIds = session->getPlayersIds();

  for (std::size_t idx{0}; idx < state.players.size(); ++idx) {
    const auto &player = state.players[idx];

    if (!player)
      continue;

    auto it = std::find(playerIds.begin(), playerIds.end(), player->id);
    int64_t playerIdx =
        (it != playerIds.end()) ? std::distance(playerIds.begin(), it) : -1;

    drawPlayerStats(*player, yOffset, gameType, playerIdx >= 0);

    if (player->lives > 0)
      drawPlayer(*player, playerColor(player->id, idx, playerIds, false),
                 playerIdx);
  }
}

void EntityDesigner::drawPlayersForPvPGame(Session *session,
                                           const shared::PvPGameState &state,
                                           GameType gameType) {
  auto yOffset = 10.0f;
  const auto playerIds = session->getPlayersIds();

  const auto ownsTeam = [&playerIds](const auto &team) {
    return team && std::any_of(team->players.begin(), team->players.end(),
                               [&playerIds](const auto &player) {
                                 return player &&
                                        std::find(playerIds.begin(),
                                                  playerIds.end(),
                                                  player->id) !=
                                            playerIds.end();
                               });
  };

  // One team of mine online: that's "my team". None or both: no opponents,
  // every ship its own colour, like local.
  std::optional<std::size_t> myTeamIdx{};
  if (gameType == GameType::ONLINE &&
      std::count_if(state.teams.begin(), state.teams.end(), ownsTeam) == 1)
    myTeamIdx = std::distance(
        state.teams.begin(),
        std::find_if(state.teams.begin(), state.teams.end(), ownsTeam));

  for (std::size_t teamIdx{0}; teamIdx < state.teams.size(); ++teamIdx) {
    const auto &team = state.teams[teamIdx];

    if (!team)
      continue;

    if (shared::count_optional(team->players) > 1) {
      const auto teamLabel = std::string("Team ") + std::to_string(teamIdx + 1);
      DrawText(teamLabel.c_str(),
               shared::SCREEN_WIDTH - 10 -
                   MeasureText(teamLabel.c_str(), STATUS_FONT_SIZE),
               yOffset, STATUS_FONT_SIZE, teamIdx == 0 ? SKYBLUE : PINK);
      yOffset += STATUS_FONT_SIZE + 10;
    }

    for (std::size_t rank{0}; rank < team->players.size(); ++rank) {
      const auto &player = team->players[rank];

      if (!player)
        continue;

      auto it = std::find(playerIds.begin(), playerIds.end(), player->id);
      int64_t playerIdxOnThisMachine =
          (it != playerIds.end()) ? std::distance(playerIds.begin(), it) : -1;

      const auto color = playerColor(
          player->id, teamIdx * shared::MAX_PLAYERS_PER_TEAM + rank, playerIds,
          myTeamIdx && *myTeamIdx != teamIdx);

      drawPlayerStats(*player, yOffset, gameType, playerIdxOnThisMachine >= 0);

      if (player->lives > 0)
        drawPlayer(*player, color, playerIdxOnThisMachine);
    }
  }
}

void EntityDesigner::drawStars(
    const std::array<Star, STAR_COUNT> &stars) const {
  for (const auto &star : stars) {
    DrawCircleV(star.position, star.size, star.color);
  }
}

void EntityDesigner::drawParticles(
    const std::array<Particle, MAX_PARTICLES> &particles) const {
  for (const auto &p : particles) {
    if (p.lifetime <= 0.0f)
      continue;

    float ratio = p.lifetime / p.max_lifetime;
    if (ratio < 0.0f)
      ratio = 0.0f;
    if (ratio > 1.0f)
      ratio = 1.0f;

    Color c = p.color;
    c.a = static_cast<unsigned char>(255.0f * ratio);

    if (p.size <= 3.0f) {
      DrawCircleV(p.position, p.size, c);
    } else {
      DrawRectangleV(
          {p.position.x - p.size / 2.0f, p.position.y - p.size / 2.0f},
          {p.size, p.size}, c);
    }
  }
}

void EntityDesigner::drawBullet(const shared::BulletState &state) const {
  if (!state.active)
    return;

  auto screenPosition = toScreen(state.position);

  DrawRectangle((int)screenPosition.x - shared::BulletSimState::WIDTH / 2,
                (int)screenPosition.y - shared::BulletSimState::HEIGHT / 2,
                shared::BulletSimState::WIDTH, shared::BulletSimState::HEIGHT,
                YELLOW);
}

Color EntityDesigner::playerColor(shared::PlayerId id, std::size_t slot,
                                  const ThisMachinePlayerIds &playerIds,
                                  bool isOpponent) {
  if (isOpponent)
    return PLAYER_ENEMY_COLOR;

  const auto it = std::find(playerIds.begin(), playerIds.end(), id);
  if (it == playerIds.begin())
    return MAIN_PLAYER_COLOR;

  // This machine's players take the first colours, everyone else follows by
  // slot, so the two groups never collide.
  const auto idx = it != playerIds.end()
                       ? static_cast<std::size_t>(
                             std::distance(playerIds.begin(), it)) -
                             1
                       : shared::MAX_PLAYERS_PER_CLIENT - 1 + slot;
  return PLAYER_COLORS[idx % PLAYER_COLORS.size()];
}
