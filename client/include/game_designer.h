#pragma once

#include "elements/text_input.h"
#include "entity_designer.h"
#include "shared/aliases.h"
#include <string>
#include <vector>

class GameDesigner {
private:
  EntityDesigner entity_designer_{};

  static constexpr std::array<Color, shared::MAX_TEAMS> TEAMS_COLORS{SKYBLUE,
                                                                     PINK};

  static constexpr int TITLE_FONT_SIZE = 64;
  static constexpr int HEADING_FONT_SIZE = 28;
  static constexpr int OPTION_FONT_SIZE = 24;
  static constexpr int BODY_FONT_SIZE = 20;
  static constexpr int HINT_FONT_SIZE = 18;

  static constexpr float LINE_GAP = 8.0f;
  static constexpr float SECTION_GAP = 36.0f;
  static constexpr float ROW_GAP = 10.0f;
  static constexpr float OPTION_SPACING = 30.0f;
  static constexpr float ROSTER_COLUMN_GAP = 24.0f;

  /// Top of the header block every selection screen shares, so they all
  /// start at the same height instead of each picking its own offset.
  static constexpr float SELECTION_TOP_Y = shared::SCREEN_HEIGHT / 2.0f - 140;

  static constexpr float BORDER_MIN_THICKNESS = 3.0f;
  static constexpr float BORDER_MAX_THICKNESS = 9.0f;

private:
  /// Draws @p text horizontally centred on the screen with its top at @p y.
  /// @return The y immediately below the drawn line, so callers can chain
  /// rows by adding their own gap rather than tracking absolute offsets.
  static float drawCentered(const char *text, float y, int fontSize,
                            Color color);

  /// Draws the instruction + key-hint pair shared by every selection screen.
  /// @return The y at which that screen's options should start.
  static float drawSelectionHeader(const char *instructions, float y);

  /// Draws @p options as one horizontally centred row, highlighting
  /// @p selectedIdx. Centres the row as a whole, so adding or removing an
  /// option keeps it balanced.
  static float drawOptionRow(const std::vector<std::string> &options,
                             std::size_t selectedIdx, float y);

  /// Draws a two-column roster row: @p label left of centre, @p status
  /// right of it. Used by both lobby branches and the end screen so every
  /// player list lines up identically.
  static float drawRosterRow(const std::string &label, const std::string &status,
                             Color statusColor, float y);

  /// @return @p state's display name, falling back to "Player N" when the
  /// server sent none, suffixed with " (You)" when @p markAsYours.
  static std::string playerLabel(const shared::PlayerState &state,
                                 bool markAsYours);

  /// Pulsing border + dimmed backdrop behind the end screen, driven by
  /// `Game::score_anim_time_`.
  static void drawEndScreenBackdrop(float animTime);

public:
  void drawMainMenu(GameType gameType) const;
  void drawModeSelection(const Config *config) const;

  void
  drawPlayerCountSelectionCoop(shared::PlayerCount currentPlayerCount) const;

  void drawPvPSchemaSelection(
      uint8_t currentTeamCount, shared::PlayerCount currentTeamSize,
      const std::vector<std::pair<shared::PlayerCount, shared::PlayerCount>>
          &matchUps) const;

  void
  drawOnlinePlayerCountSelection(shared::PlayerCount currentPlayerCount) const;

  void
  drawTextInputs(std::array<TextInput, shared::MAX_PLAYERS_PER_CLIENT> &inputs,
                 shared::PlayerCount playersCount) const;

  void drawConnectingScreen() const;
  void drawLobby(OnlineSession *session, bool areReady) const;

  /// Final results for whichever mode just ended: coop WON/GAME_OVER with
  /// per-player scores, or the PvP per-team outcome from this machine's
  /// own perspective. @p animTime drives the pulsing border.
  void drawEndScreen(const shared::GameState &state, Session *session,
                     GameType gameType, float animTime) const;
  void drawGame(const shared::GameState &state, Session *session,
                GameType gameType, GameScreen screen,
                const std::array<Particle, MAX_PARTICLES> &particles);

  void drawStars(const std::array<Star, STAR_COUNT> &stars);

  void init();
  void unload();
};
