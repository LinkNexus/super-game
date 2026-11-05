#pragma once

#include "raylib.h"
#include "session.h"
#include "shared/messages.h"
#include "types.h"

class EntityDesigner {
private:
  Texture2D boss_texture_{};
  Texture2D player_ship_texture_{};
  Texture2D player_live_texture_{};
  std::array<Texture2D, 2> enemies_textures_{};

  static constexpr float BOSS_HEALTH_BAR_WIDTH = 300.0f;
  static constexpr float BOSS_HEALTH_BAR_HEIGHT = 20.0f;
  static constexpr float BOSS_HEALTH_BAR_Y = 10.0f;

  static constexpr float HEART_SIZE = 32.0f;
  static constexpr float HEART_SPACING = 4.0f;

  static constexpr float MARKER_FONT_SIZE = 10.0f;
  static constexpr float MARKER_GAP = 6.0f;

  static constexpr Color MAIN_PLAYER_COLOR{WHITE};
  static constexpr std::array<Color, 5> PLAYER_COLORS{GREEN, SKYBLUE, PURPLE,
                                                      YELLOW, GOLD};
  static constexpr auto PLAYER_ENEMY_COLOR{RED};

  /// Depends on ids and roster slots only, never on who's alive, so a ship
  /// keeps its colour all match. `slot`: coop, index in `players`; PvP,
  /// teamIdx * MAX_PLAYERS_PER_TEAM + rank. Same rule as the web client.
  static Color playerColor(shared::PlayerId id, std::size_t slot,
                           const ThisMachinePlayerIds &playerIds,
                           bool isOpponent);

private:
  void drawBossHealthBar(const shared::BossState &state) const;

  void drawPlayer(const shared::PlayerState &state, Color type,
                  int64_t playerIdx) const;
  void drawLives(const shared::PlayerState &state, float &yOffset) const;
  void drawPlayerStats(const shared::PlayerState &state, float &yOffset,
                       GameType gameType, bool isOnThisMachine) const;

  void drawEnemy(Vector2 pos, shared::EnemyType type) const;

public:
  void loadTextures();
  void unloadTextures();

  void drawBoss(const shared::BossState &state, bool canDrawHealthBar) const;

  void drawEnemies(const shared::EnemiesPoolState &enemies,
                   mathlib::Vec2f offsetX) const;

  void drawPlayersForPvPGame(Session *session,
                             const shared::PvPGameState &state,
                             GameType gameType);

  void drawPlayersForCoopGame(const shared::CoopGameState &state,
                              Session *session, GameType gameType) const;

  void drawStars(const std::array<Star, STAR_COUNT> &stars) const;

  void
  drawParticles(const std::array<Particle, MAX_PARTICLES> &particles) const;

  void drawBullet(const shared::BulletState &state) const;
};
