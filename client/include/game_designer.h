#pragma once

#include "elements/text_input.h"
#include "entity_designer.h"
#include "shared/aliases.h"

class GameDesigner {
private:
  EntityDesigner entity_designer_{};

  static constexpr std::array<Color, shared::MAX_TEAMS> TEAMS_COLORS{SKYBLUE,
                                                                     PINK};

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
  void drawEndScreen() const;
  void drawGame(const shared::GameState &state, Session *session,
                GameType gameType, GameScreen screen,
                const std::array<Particle, MAX_PARTICLES> &particles);

  void drawStars(const std::array<Star, STAR_COUNT> &stars);

  void init();
  void unload();
};
