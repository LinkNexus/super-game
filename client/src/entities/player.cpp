#include "entities/player.h"
#include "constants.h"
#include "raylib.h"
#include "session.h"
#include "shared/constants.h"
#include "shared/math_utils.h"
#include "shared/messages.h"
#include "shared/sim/game_sim.h"
#include "shared/sim/player_sim.h"
#include "utils.h"
#include <string>

Texture2D Player::heart_texture_ = {};

void Player::loadTexture() {
  texture = LoadTexture("assets/playerShip.png");

  heart_texture_ = LoadTexture("assets/heart.png");
}

void Player::unload() {
  UnloadTexture(texture);
  if (heart_texture_.id != 0) {
    UnloadTexture(heart_texture_);
  }
}

void Player::drawPlayer(const shared::Vec2D position, float orientation,
                        Type type) const {
  auto screenPosition = toScreen(position);
  const auto size = shared::PlayerSimState::SIZE;

  if (texture.id != 0) {
    float scale = (size * 2.0f) / texture.width;
    Vector2 draw_pos = {screenPosition.x - (texture.width * scale) / 2.0f,
                        screenPosition.y - (texture.height * scale) / 2.0f};

    Color tint;
    switch (type) {
    case Type::MYSELF:
      tint = WHITE;
      break;
    case Type::PARTNER:
      tint = SKYBLUE;
      break;
    case Type::ENEMY:
      tint = RED;
      break;
    }

    Vector2 origin = {texture.width * scale / 2.0f,
                      texture.height * scale / 2.0f};
    Rectangle source = {0, 0, (float)texture.width, (float)texture.height};
    Rectangle dest = {screenPosition.x, screenPosition.y,
                      (float)texture.width * scale,
                      (float)texture.height * scale};
    DrawTexturePro(texture, source, dest, origin,
                   shared::toDegrees(orientation), tint);
  } else {
    Vector2 tip = {screenPosition.x, screenPosition.y - size};
    Vector2 left = {screenPosition.x - size * 0.7f,
                    screenPosition.y + size * 0.7f};
    Vector2 right = {screenPosition.x + size * 0.7f,
                     screenPosition.y + size * 0.7f};

    Color shipColor;
    switch (type) {
    case Type::MYSELF:
      shipColor = PURPLE;
      break;
    case Type::PARTNER:
      shipColor = SKYBLUE;
      break;
    case Type::ENEMY:
      shipColor = RED;
      break;
    }

    DrawTriangle(tip, left, right, shipColor);
    DrawTriangleLines(tip, left, right, WHITE);
  }
}

void Player::drawLives(uint8_t lives, float yOffset) const {
  if (lives == 0) {
    auto text = "Dead";
    DrawText(text,
             shared::SCREEN_WIDTH - 5 - MeasureText(text, STATUS_FONT_SIZE) -
                 HEART_SIZE,
             yOffset + (HEART_SIZE - STATUS_FONT_SIZE) / 2, STATUS_FONT_SIZE,
             WHITE);
    return;
  }

  auto livesText = std::to_string(lives);
  auto textWidth = MeasureText(livesText.c_str(), STATUS_FONT_SIZE);
  auto heartScale = HEART_SIZE / static_cast<float>(heart_texture_.width);
  float heartX = shared::SCREEN_WIDTH - 10 - HEART_SIZE;
  float textX = heartX - 5 - textWidth;

  DrawText(livesText.c_str(), textX,
           yOffset + (HEART_SIZE - STATUS_FONT_SIZE) / 2, STATUS_FONT_SIZE,
           WHITE);

  if (heart_texture_.id != 0) {
    DrawTextureEx(heart_texture_, {heartX, yOffset - 3}, 0.0f, heartScale,
                  WHITE);
  } else {
    livesText = livesText + (lives > 1 ? " lives" : " life");
    DrawText(livesText.c_str(),
             shared::SCREEN_WIDTH - 10 -
                 MeasureText(livesText.c_str(), STATUS_FONT_SIZE),
             yOffset, STATUS_FONT_SIZE, WHITE);
  }
}

void Player::draw(Session *currentSession,
                  const shared::OptionalTypeInPlayerSlots<shared::PlayerState>
                      &states) const {
  float yOffset = 10.0f;
  bool isOnline = false;

  if (OnlineSession *session = dynamic_cast<OnlineSession *>(currentSession))
    isOnline = true;

  for (std::size_t idx = 0; idx < states.size(); ++idx) {
    const auto &player = states[idx];

    shared::PlayerId playerId =
        isOnline ? ((OnlineSession *)currentSession)->getPlayerId() : 1;

    if (player.has_value()) {
      bool isMain = false;

      if (player->id == playerId) {
        isMain = true;
      }

      auto type = isOnline ? isMain ? Type::MYSELF : Type::PARTNER
                  : player->id == playerId ? Type::MYSELF
                                           : Type::ENEMY;

      drawPlayerAndStats(player.value(), type, isOnline, idx, yOffset);
    }
  }
}

void Player::draw(Session *currentSession,
                  const std::array<shared::TeamState, 2> &state,
                  uint8_t teamSize) const {
  float yOffset = 10.0f;
  bool isOnline = false;

  if (OnlineSession *session = dynamic_cast<OnlineSession *>(currentSession))
    isOnline = true;

  for (std::size_t teamIdx = 0; teamIdx < state.size(); ++teamIdx) {
    const auto &team = state[teamIdx];

    if (teamSize > 1) {
      const std::string teamLabel = "Team " + std::to_string(teamIdx + 1);
      DrawText(teamLabel.c_str(),
               shared::SCREEN_WIDTH - 10 -
                   MeasureText(teamLabel.c_str(), STATUS_FONT_SIZE),
               yOffset, STATUS_FONT_SIZE, WHITE);
      yOffset += STATUS_FONT_SIZE + 10;
    }

    shared::PlayerId currentPlayerId = 0;
    shared::PvPGameSim::TeamId currentPlayerTeamId = 0;

    if (isOnline) {
      currentPlayerId = ((OnlineSession *)currentSession)->getPlayerId();
      auto team = std::find_if(
          state.begin(), state.end(), [&currentPlayerId](const auto &t) {
            return std::any_of(t.players.begin(), t.players.end(),
                               [&currentPlayerId](const auto &p) {
                                 return p.has_value() &&
                                        p->id == currentPlayerId;
                               });
          });

      if (team != state.end()) {
        currentPlayerTeamId = team->id;
      }
    } else {
      if (team.id == 1) {
        currentPlayerId = 2;
      }
    }

    for (std::size_t playerIdx = 0; playerIdx < team.players.size();
         ++playerIdx) {
      const auto &player = team.players[playerIdx];
      if (!player.has_value())
        continue;
      Type type;

      if (player->id == currentPlayerId)
        type = Type::MYSELF;
      else {
        if (!isOnline)
          type = Type::ENEMY;
        else {
          if (team.id == currentPlayerTeamId)
            type = Type::PARTNER;
          else
            type = Type::ENEMY;
        }
      }

      drawPlayerAndStats(player.value(), type, isOnline, playerIdx, yOffset);
    }
  }
}

void Player::drawPlayerAndStats(const shared::PlayerState &player, Type type,
                                bool isOnline, std::size_t idx,
                                float &yOffset) const {
  if (player.lives > 0)
    drawPlayer(player.position, player.orientation, type);

  const std::string playerLabel =
      (player.name.empty() ? "Player " + std::to_string(idx + 1)
                           : player.name) +
      (!isOnline ? "" : ((type == Type::MYSELF ? " (You)" : "")));

  DrawText(playerLabel.c_str(),
           shared::SCREEN_WIDTH - 10 -
               MeasureText(playerLabel.c_str(), STATUS_FONT_SIZE),
           yOffset, STATUS_FONT_SIZE, WHITE);
  yOffset += STATUS_FONT_SIZE;

  drawLives(player.lives, yOffset);
  yOffset += HEART_SIZE;

  const char *pointsText = TextFormat("Points: %d", player.points);
  DrawText(pointsText,
           shared::SCREEN_WIDTH - 10 -
               MeasureText(pointsText, STATUS_FONT_SIZE),
           yOffset, STATUS_FONT_SIZE, WHITE);
  yOffset += STATUS_FONT_SIZE + 5;

  DrawLine(shared::SCREEN_WIDTH - MeasureText(pointsText, STATUS_FONT_SIZE) -
               10,
           yOffset, shared::SCREEN_WIDTH, yOffset, WHITE);
  yOffset += 25;
}
