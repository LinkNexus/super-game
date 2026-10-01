#include "game_designer.h"
#include "constants.h"
#include "session.h"
#include "shared/aliases.h"
#include "shared/constants.h"
#include "shared/helpers.h"
#include "shared/messages.h"
#include "shared/sim/game_sim.h"
#include "types.h"
#include <cstdint>
#include <string>
#include <variant>

void GameDesigner::drawMainMenu(GameType gameType) const {
  const std::string title = "THE SUPER GAME";
  const int title_size = 64;
  const int title_x =
      (shared::SCREEN_WIDTH - MeasureText(title.c_str(), title_size)) / 2;
  DrawText(title.c_str(), title_x, 120, title_size, YELLOW);

  const std::string subtitle = "Press ENTER to play";
  const int subtitle_x =
      (shared::SCREEN_WIDTH - MeasureText(subtitle.c_str(), 24)) / 2;
  DrawText(subtitle.c_str(), subtitle_x, 220, 24, WHITE);

  DrawText("Mode selection:", shared::SCREEN_WIDTH / 2 - 180,
           shared::SCREEN_HEIGHT / 2 - 40, 20, LIGHTGRAY);
  DrawText("Local", shared::SCREEN_WIDTH / 2 - 150,
           shared::SCREEN_HEIGHT / 2 + 40, 28,
           gameType == GameType::LOCAL ? YELLOW : WHITE);
  DrawText("Online", shared::SCREEN_WIDTH / 2 + 50,
           shared::SCREEN_HEIGHT / 2 + 40, 28,
           gameType == GameType::ONLINE ? YELLOW : WHITE);

  DrawText("Controls:", shared::SCREEN_WIDTH / 2 - 210,
           shared::SCREEN_HEIGHT / 2 + 120, 22, LIGHTGRAY);
  DrawText("- LEFT / RIGHT: move", shared::SCREEN_WIDTH / 2 - 180,
           shared::SCREEN_HEIGHT / 2 + 150, 20, WHITE);
  DrawText("- SPACE: shoot", shared::SCREEN_WIDTH / 2 - 180,
           shared::SCREEN_HEIGHT / 2 + 180, 20, WHITE);
  DrawText("- A / D / W: second player (dual)", shared::SCREEN_WIDTH / 2 - 180,
           shared::SCREEN_HEIGHT / 2 + 210, 20, WHITE);
  DrawText("- ESC: cancel / pause", shared::SCREEN_WIDTH / 2 - 180,
           shared::SCREEN_HEIGHT / 2 + 240, 20, WHITE);
}

void GameDesigner::drawModeSelection(const Config *config) const {
  const auto instructions = "Do you want to play CooP or PvP?";
  DrawText(
      instructions,
      (shared::SCREEN_WIDTH - MeasureText(instructions, STATUS_FONT_SIZE)) / 2,
      shared::SCREEN_HEIGHT / 2 - 40, 20, WHITE);

  const auto confirmText = "Use LEFT/RIGHT to choose and ENTER to confirm";
  const auto confirmTextWidth = MeasureText(confirmText, STATUS_FONT_SIZE);
  DrawText(confirmText, (shared::SCREEN_WIDTH - confirmTextWidth) / 2,
           shared::SCREEN_HEIGHT / 2 - 10, 20, WHITE);

  const auto localText = "CooP";
  const auto onlineText = "PvP";
  auto &mode = config->modes[config->mode_idx];

  DrawText(localText, (shared::SCREEN_WIDTH - confirmTextWidth) / 2,
           shared::SCREEN_HEIGHT / 2 + 40, 24,
           std::holds_alternative<CoopMode>(mode) ? YELLOW : WHITE);

  DrawText(onlineText,
           (shared::SCREEN_WIDTH - confirmTextWidth) / 2 + confirmTextWidth -
               MeasureText(onlineText, 24),
           shared::SCREEN_HEIGHT / 2 + 40, 24,
           std::holds_alternative<PvPMode>(mode) ? YELLOW : WHITE);
}

void GameDesigner::drawPlayerCountSelectionCoop(
    shared::PlayerCount currentPlayerCount) const {
  const auto instructions = "Choose the number of players for CooP";
  DrawText(
      instructions,
      (shared::SCREEN_WIDTH - MeasureText(instructions, STATUS_FONT_SIZE)) / 2,
      shared::SCREEN_HEIGHT / 2 - 40, 20, WHITE);

  const auto confirmText = "Use LEFT/RIGHT to choose and ENTER to confirm";
  const auto confirmTextWidth = MeasureText(confirmText, STATUS_FONT_SIZE);
  DrawText(confirmText, (shared::SCREEN_WIDTH - confirmTextWidth) / 2,
           shared::SCREEN_HEIGHT / 2 - 10, 20, WHITE);

  auto offsetX = 0.0f;

  for (std::size_t playerCount = 1; playerCount <= MAX_PLAYERS_ON_THIS_MACHINE;
       ++playerCount) {
    std::string playerCountText{std::to_string(playerCount)};

    DrawText(playerCountText.c_str(),
             (shared::SCREEN_WIDTH - confirmTextWidth) / 2 + offsetX,
             shared::SCREEN_HEIGHT / 2 + 40, 24,
             playerCount == currentPlayerCount ? YELLOW : WHITE);

    offsetX += MeasureText(playerCountText.c_str(), 24) + 20.0f;
  }
}

void GameDesigner::drawPvPSchemaSelection(
    uint8_t currentTeamCount, shared::PlayerCount currentTeamSize,
    const std::vector<std::pair<shared::PlayerCount, shared::PlayerCount>>
        &matchUps) const {
  const auto instructions = "Choose the number of players per team for PvP";
  DrawText(
      instructions,
      (shared::SCREEN_WIDTH - MeasureText(instructions, STATUS_FONT_SIZE)) / 2,
      shared::SCREEN_HEIGHT / 2 - 40, 20, WHITE);

  const auto confirmText = "Use LEFT/RIGHT to choose and ENTER to confirm";
  const auto confirmTextWidth = MeasureText(confirmText, STATUS_FONT_SIZE);
  const auto confirmTextOffsetX = (shared::SCREEN_WIDTH - confirmTextWidth) / 2;
  DrawText(confirmText, confirmTextOffsetX, shared::SCREEN_HEIGHT / 2 - 10, 20,
           WHITE);

  std::vector<std::string> matchUpTexts;
  for (const auto &kvp : matchUps) {
    std::string matchUpText{std::to_string(kvp.second)};
    for (std::size_t i = 1; i < kvp.first; ++i) {
      matchUpText += " Vs " + std::to_string(kvp.second);
    }
    matchUpTexts.push_back(matchUpText);
  }

  auto offsetX = 0.0f;

  for (std::size_t i = 0; i < matchUps.size(); ++i) {
    const auto &[teamCount, teamSize] = *std::next(matchUps.begin(), i);
    const auto &matchUpText = matchUpTexts[i];

    DrawText(matchUpText.c_str(), confirmTextOffsetX + offsetX,
             shared::SCREEN_HEIGHT / 2 + 40, 24,
             teamSize == currentTeamSize && currentTeamCount == teamCount
                 ? YELLOW
                 : WHITE);

    offsetX += MeasureText(matchUpText.c_str(), 24) + 20.0f;
  }
}

void GameDesigner::drawTextInputs(
    std::array<TextInput, shared::MAX_PLAYERS_PER_CLIENT> &inputs,
    shared::PlayerCount playersCount) const {
  auto offsetY{0.0f};

  for (std::size_t i = 0; i < playersCount; ++i) {
    auto &box = inputs[i];
    box.rect = {(shared::SCREEN_WIDTH - TextInput::WIDTH) / 2.0f,
                200.0f + offsetY, TextInput::WIDTH, TextInput::HEIGHT};
    offsetY += box.draw() + 20.0f;
  }

  const auto text{"Choose a name for the online game. Press Enter to continue"};
  DrawText(text, (shared::SCREEN_WIDTH - MeasureText(text, 20)) / 2,
           inputs[0].rect.y + offsetY, 20, WHITE);
}

void GameDesigner::drawConnectingScreen() const {
  DrawText("Connecting to server...", shared::SCREEN_WIDTH / 2 - 170,
           shared::SCREEN_HEIGHT / 2, 24, LIGHTGRAY);
  DrawText("Press ESC to cancel", shared::SCREEN_WIDTH / 2 - 140,
           shared::SCREEN_HEIGHT / 2 + 40, 20, WHITE);
}

void GameDesigner::drawLobby(OnlineSession *session, bool areReady) const {
  const auto &lobbyUpdate{session->getLobbyUpdate()};

  const auto title{"Lobby"};
  float titleY = shared::SCREEN_HEIGHT / 2.0f - 100;
  DrawText(title, (shared::SCREEN_WIDTH - MeasureText(title, 32)) / 2, titleY,
           32, WHITE);

  auto y{titleY + 60};

  std::visit(
      shared::overloaded{
          [&y, &session](const shared::CoopGameLobbyUpdate &u) {
            auto readyCount{0};

            for (std::size_t i = 0; i < u.max_players_count; ++i) {
              if (i >= u.players_count) {
                const auto waitingText{"Waiting for a player..."};
                DrawText(waitingText,
                         (shared::SCREEN_WIDTH - MeasureText(waitingText, 18)) /
                             2,
                         y, 18, DARKGRAY);
                y += 28;
                continue;
              }

              const auto &p = u.players[i];
              if (p.is_ready)
                ++readyCount;

              const auto status{p.is_ready ? "READY" : "Waiting..."};
              auto statusColor{p.is_ready ? GREEN : LIGHTGRAY};

              auto playerIds = session->getPlayersIds();
              auto label{p.name + (std::find(playerIds.begin(), playerIds.end(),
                                             p.id) != playerIds.end()
                                       ? " (You)"
                                       : "")};
              auto labelWidth{MeasureText(label.c_str(), 18)};
              auto statusWidth{MeasureText(status, 18)};

              DrawText(label.c_str(),
                       (shared::SCREEN_WIDTH - labelWidth - statusWidth) / 2, y,
                       18, WHITE);
              DrawText(status,
                       (shared::SCREEN_WIDTH + labelWidth - statusWidth) / 2, y,
                       18, statusColor);
              y += 28;
            }

            y += 12;
            const auto progress{TextFormat("Players ready: %d/%d", readyCount,
                                           u.max_players_count)};
            DrawText(progress,
                     (shared::SCREEN_WIDTH - MeasureText(progress, 18)) / 2, y,
                     18, LIGHTGRAY);
            y += 32;
          },
          [&y, &session](const shared::PvPGameLobbyUpdate &u) {
            std::string subtitle{std::to_string(u.team_size)};
            for (std::size_t i = 1; i < u.team_count; ++i) {
              subtitle += " Vs " + std::to_string(u.team_size);
            }

            DrawText(
                subtitle.c_str(),
                (shared::SCREEN_WIDTH - MeasureText(subtitle.c_str(), 18)) / 2,
                y, 18, DARKGRAY);
            y += 36;
          }},
      lobbyUpdate);

  const auto prompt{areReady ? "Press SPACE to cancel ready"
                             : "Press SPACE to ready up"};
  DrawText(prompt, (shared::SCREEN_WIDTH - MeasureText(prompt, 18)) / 2, y, 18,
           LIGHTGRAY);

  const auto leave_prompt{"Press ESC to leave the lobby"};
  DrawText(leave_prompt,
           (shared::SCREEN_WIDTH - MeasureText(leave_prompt, 18)) / 2, y + 32,
           18, LIGHTGRAY);
}

void GameDesigner::drawGame(
    const shared::GameState &state, Session *session, GameType gameType,
    GameScreen screen, const std::array<Particle, MAX_PARTICLES> &particles) {
  std::visit(shared::overloaded{
                 [&](const shared::CoopGameState &s) {
                   entity_designer_.drawPlayersForCoopGame(s, session,
                                                           gameType);

                   for (const auto &b : s.bullets)
                     entity_designer_.drawBullet(b);

                   entity_designer_.drawParticles(particles);

                   entity_designer_.drawEnemies(s.enemies, s.enemies_offset_x,
                                                s.enemies_offset_y);

                   {
                     using GamePhase = shared::CoopGameSim::Phase;
                     entity_designer_.drawBoss(
                         s.boss, static_cast<GamePhase>(s.phase) ==
                                     GamePhase::FIGHT_BOSS);
                   }
                 },
                 [&](const shared::PvPGameState &s) {
                   entity_designer_.drawPlayersForPvPGame(session, s, gameType);

                   for (const auto &b : s.bullets)
                     entity_designer_.drawBullet(b);

                   // entity_designer_.drawParticles(particles);
                 }},
             state);

  if (screen == GameScreen::PAUSED) {
    DrawRectangle(0, 0, shared::SCREEN_WIDTH, shared::SCREEN_HEIGHT,
                  Fade(BLACK, 0.6f));

    DrawText("Game Paused", shared::SCREEN_WIDTH / 2 - 100,
             shared::SCREEN_HEIGHT / 2 - 20, 32, WHITE);

    DrawText("Press ESC to resume", shared::SCREEN_WIDTH / 2 - 100,
             shared::SCREEN_HEIGHT / 2 + 30, 20, WHITE);
  }
}

void GameDesigner::drawEndScreen() const {}

void GameDesigner::init() { entity_designer_.loadTextures(); }

void GameDesigner::unload() { entity_designer_.unloadTextures(); }

void GameDesigner::drawOnlinePlayerCountSelection(
    shared::PlayerCount currentPlayerCount) const {
  const auto instructions =
      "Choose the number of players that will play on this machine";
  DrawText(
      instructions,
      (shared::SCREEN_WIDTH - MeasureText(instructions, STATUS_FONT_SIZE)) / 2,
      shared::SCREEN_HEIGHT / 2 - 40, 20, WHITE);

  const auto confirmText = "Use LEFT/RIGHT to choose and ENTER to confirm";
  const auto confirmTextWidth = MeasureText(confirmText, STATUS_FONT_SIZE);
  DrawText(confirmText, (shared::SCREEN_WIDTH - confirmTextWidth) / 2,
           shared::SCREEN_HEIGHT / 2 - 10, 20, WHITE);

  auto offsetX = 0.0f;

  for (std::size_t playerCount = 1;
       playerCount <= shared::MAX_PLAYERS_PER_CLIENT; ++playerCount) {
    std::string playerCountText{std::to_string(playerCount)};

    DrawText(playerCountText.c_str(),
             (shared::SCREEN_WIDTH - confirmTextWidth) / 2 + offsetX,
             shared::SCREEN_HEIGHT / 2 + 40, 24,
             playerCount == currentPlayerCount ? YELLOW : WHITE);

    offsetX += MeasureText(playerCountText.c_str(), 24) + 20.0f;
  }
}

void GameDesigner::drawStars(const std::array<Star, STAR_COUNT> &stars) {
  entity_designer_.drawStars(stars);
}
