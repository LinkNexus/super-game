#pragma once

#include "constants.h"
#include "raylib.h"
#include "session.h"
#include "shared/constants.h"
#include "shared/messages.h"
#include <string>

/// Render-only player entity: texture + HUD (name/lives/points), no
/// simulation logic. Draws from the trimmed wire `PlayerState`, so
/// rendering is identical regardless of whether the state came from local
/// sim or a network snapshot.
struct Player {
  enum class Type { MYSELF, PARTNER, ENEMY };

  Texture2D texture = {};

  void loadTexture();
  void unload();

  static Texture2D heart_texture_;
  static constexpr float HEART_SIZE = 32.0f;
  static constexpr float HEART_SPACING = 4.0f;

private:
  void drawPlayerAndStats(const shared::PlayerState &player, Type type,
                          bool isOnline, std::size_t idx, float &yOffset) const;
  void drawLives(uint8_t lives, float yOffset) const;
  void drawPlayer(const shared::Vec2D position, float orientation,
                  Type type) const;

public:
  void draw(Session *currentSession,
            const std::array<shared::TeamState, 2> &state,
            uint8_t teamSize) const;

  /// Draws every populated slot in @p states: the ship sprite (only while
  /// alive) plus a HUD block (name, lives, points). @p current_session is
  /// used only to tell which player is "this client" in online mode
  /// (highlighted sprite, " (You)" suffix) - local mode has no such
  /// distinction. Falls back to "Player N" when a slot has no name set
  /// (always the case in local mode, since names are an online-only
  /// concept assigned by the server).
  void draw(Session *currentSession,
            const shared::OptionalTypeInPlayerSlots<shared::PlayerState>
                &states) const;
};
