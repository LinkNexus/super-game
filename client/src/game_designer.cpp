#include "game_designer.h"
#include "constants.h"
#include "session.h"
#include "shared/aliases.h"
#include "shared/constants.h"
#include "shared/helpers.h"
#include "shared/messages.h"
#include "shared/sim/game_sim.h"
#include "types.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <variant>

float GameDesigner::drawCentered(const char *text, float y, int fontSize,
                                 Color color) {
  DrawText(text, (shared::SCREEN_WIDTH - MeasureText(text, fontSize)) / 2,
           static_cast<int>(y), fontSize, color);
  return y + fontSize;
}

float GameDesigner::drawSelectionHeader(const char *instructions, float y) {
  y = drawCentered(instructions, y, HEADING_FONT_SIZE, WHITE) + LINE_GAP;
  y = drawCentered("Use LEFT / RIGHT to choose, ENTER to confirm", y,
                   HINT_FONT_SIZE, LIGHTGRAY);
  return y + SECTION_GAP;
}

float GameDesigner::drawOptionRow(const std::vector<std::string> &options,
                                  std::size_t selectedIdx, float y) {
  auto totalWidth{0.0f};
  for (const auto &option : options)
    totalWidth += MeasureText(option.c_str(), OPTION_FONT_SIZE);
  if (!options.empty())
    totalWidth += OPTION_SPACING * (options.size() - 1);

  auto x{(shared::SCREEN_WIDTH - totalWidth) / 2.0f};

  for (std::size_t i = 0; i < options.size(); ++i) {
    const auto &option = options[i];
    const auto selected = i == selectedIdx;

    DrawText(option.c_str(), static_cast<int>(x), static_cast<int>(y),
             OPTION_FONT_SIZE, selected ? YELLOW : WHITE);

    const auto width = MeasureText(option.c_str(), OPTION_FONT_SIZE);
    if (selected) {
      // Underline the selection, so it reads as chosen even where the
      // yellow-vs-white contrast is subtle.
      DrawRectangle(static_cast<int>(x),
                    static_cast<int>(y) + OPTION_FONT_SIZE + 6, width, 2,
                    YELLOW);
    }

    x += width + OPTION_SPACING;
  }

  return y + OPTION_FONT_SIZE;
}

float GameDesigner::drawRosterRow(const std::string &label,
                                  const std::string &status, Color statusColor,
                                  float y) {
  const auto labelWidth{MeasureText(label.c_str(), BODY_FONT_SIZE)};
  const auto statusWidth{MeasureText(status.c_str(), BODY_FONT_SIZE)};
  const auto left{
      (shared::SCREEN_WIDTH - labelWidth - statusWidth - ROSTER_COLUMN_GAP) /
      2.0f};

  DrawText(label.c_str(), static_cast<int>(left), static_cast<int>(y),
           BODY_FONT_SIZE, WHITE);
  DrawText(status.c_str(),
           static_cast<int>(left + labelWidth + ROSTER_COLUMN_GAP),
           static_cast<int>(y), BODY_FONT_SIZE, statusColor);

  return y + BODY_FONT_SIZE + ROW_GAP;
}

std::string GameDesigner::playerLabel(const shared::PlayerState &state,
                                      bool markAsYours) {
  auto label{state.name.empty() ? "Player " + std::to_string(state.id)
                                : state.name};
  if (markAsYours)
    label += " (You)";
  return label;
}

void GameDesigner::drawMainMenu(GameType gameType) const {
  auto y{drawCentered("THE SUPER GAME", 120, TITLE_FONT_SIZE, YELLOW) +
         LINE_GAP};
  y = drawCentered("Press ENTER to play", y, OPTION_FONT_SIZE, WHITE) +
      SECTION_GAP;

  y = drawCentered("Mode selection:", y, BODY_FONT_SIZE, LIGHTGRAY) + LINE_GAP;
  y = drawOptionRow({"Local", "Online"}, gameType == GameType::LOCAL ? 0 : 1,
                    y) +
      SECTION_GAP + LINE_GAP;

  static constexpr std::array<const char *, 4> CONTROLS{
      "LEFT / RIGHT : move", "SPACE : shoot",
      "A / D / W : second player (dual)", "ESC : cancel / pause"};

  y = drawCentered("Controls:", y, BODY_FONT_SIZE, LIGHTGRAY) + LINE_GAP;

  // Left-align the list, but centre it as a block on its widest line, so it
  // reads as a list instead of four independently centred sentences.
  auto widest{0};
  for (const auto *line : CONTROLS)
    widest = std::max(widest, MeasureText(line, BODY_FONT_SIZE));

  const auto left{(shared::SCREEN_WIDTH - widest) / 2};
  for (const auto *line : CONTROLS) {
    DrawText(line, left, static_cast<int>(y), BODY_FONT_SIZE, WHITE);
    y += BODY_FONT_SIZE + ROW_GAP;
  }
}

void GameDesigner::drawModeSelection(const Config *config) const {
  auto y{
      drawSelectionHeader("Do you want to play CooP or PvP?", SELECTION_TOP_Y)};
  drawOptionRow({"CooP", "PvP"}, config->mode_idx, y);
}

void GameDesigner::drawPlayerCountSelectionCoop(
    shared::PlayerCount currentPlayerCount) const {
  auto y{drawSelectionHeader("Choose the number of players for CooP",
                             SELECTION_TOP_Y)};

  std::vector<std::string> options;
  for (std::size_t playerCount = 1; playerCount <= MAX_PLAYERS_ON_THIS_MACHINE;
       ++playerCount)
    options.push_back(std::to_string(playerCount));

  drawOptionRow(options, currentPlayerCount - 1, y);
}

void GameDesigner::drawPvPSchemaSelection(
    uint8_t currentTeamCount, shared::PlayerCount currentTeamSize,
    const std::vector<std::pair<shared::PlayerCount, shared::PlayerCount>>
        &matchUps) const {
  auto y{drawSelectionHeader("Choose the number of players per team for PvP",
                             SELECTION_TOP_Y)};

  std::vector<std::string> options;
  // Out of range until a match-up matches, so nothing is highlighted rather
  // than the first entry being highlighted by default.
  auto selectedIdx{matchUps.size()};

  for (std::size_t i = 0; i < matchUps.size(); ++i) {
    const auto &[teamCount, teamSize] = matchUps[i];

    std::string matchUpText{std::to_string(teamSize)};
    for (std::size_t t = 1; t < teamCount; ++t)
      matchUpText += " Vs " + std::to_string(teamSize);

    options.push_back(matchUpText);

    if (teamSize == currentTeamSize && teamCount == currentTeamCount)
      selectedIdx = i;
  }

  drawOptionRow(options, selectedIdx, y);
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
  const auto playerIds{session->getPlayersIds()};

  // The lobby identifies players by *connection* id on both sides, so
  // matching these against `PlayerInfo::id` is correct here - unlike the
  // in-game rosters, which carry the sim's own ids.
  const auto isOnThisMachine = [&playerIds](shared::PlayerId id) {
    return id != 0 &&
           std::find(playerIds.begin(), playerIds.end(), id) != playerIds.end();
  };

  const auto drawSlot = [&isOnThisMachine](const shared::PlayerInfo &p,
                                           std::size_t slotNumber, float y) {
    auto label{p.name.empty() ? "Player " + std::to_string(slotNumber)
                              : p.name};
    if (isOnThisMachine(p.id))
      label += " (You)";

    return drawRosterRow(label, p.is_ready ? "READY" : "Waiting...",
                         p.is_ready ? GREEN : LIGHTGRAY, y);
  };

  const auto drawEmptySlot = [](float y) {
    return drawCentered("Waiting for a player...", y, BODY_FONT_SIZE,
                        DARKGRAY) +
           ROW_GAP;
  };

  auto y{drawCentered("Lobby", shared::SCREEN_HEIGHT / 2.0f - 180,
                      HEADING_FONT_SIZE, WHITE) +
         SECTION_GAP};

  std::visit(
      shared::overloaded{
          [&](const shared::CoopGameLobbyUpdate &u) {
            auto readyCount{0};

            for (std::size_t i = 0; i < u.max_players_count; ++i) {
              if (i >= u.players_count) {
                y = drawEmptySlot(y);
                continue;
              }

              const auto &p = u.players[i];
              if (p.is_ready)
                ++readyCount;

              y = drawSlot(p, i + 1, y);
            }

            y += LINE_GAP;
            y = drawCentered(TextFormat("Players ready: %d/%d", readyCount,
                                        static_cast<int>(u.max_players_count)),
                             y, HINT_FONT_SIZE, LIGHTGRAY) +
                SECTION_GAP;
          },
          [&](const shared::PvPGameLobbyUpdate &u) {
            std::string subtitle{std::to_string(u.team_size)};
            for (std::size_t i = 1; i < u.team_count; ++i)
              subtitle += " Vs " + std::to_string(u.team_size);

            y = drawCentered(subtitle.c_str(), y, HINT_FONT_SIZE, DARKGRAY) +
                SECTION_GAP;

            auto readyCount{0};

            for (std::size_t teamIdx = 0; teamIdx < u.team_count; ++teamIdx) {
              const auto &team = u.teams[teamIdx];

              auto teamLabel{std::string("Team ") +
                             std::to_string(teamIdx + 1)};
              if (std::any_of(team.players.begin(),
                              team.players.begin() + team.players_count,
                              [&isOnThisMachine](const auto &p) {
                                return isOnThisMachine(p.id);
                              }))
                teamLabel += " (Yours)";

              y = drawCentered(teamLabel.c_str(), y, BODY_FONT_SIZE,
                               TEAMS_COLORS[teamIdx]) +
                  LINE_GAP;

              for (std::size_t slot = 0; slot < u.team_size; ++slot) {
                if (slot >= team.players_count) {
                  y = drawEmptySlot(y);
                  continue;
                }

                const auto &p = team.players[slot];
                if (p.is_ready)
                  ++readyCount;

                y = drawSlot(p, slot + 1, y);
              }

              y += ROW_GAP;
            }

            y = drawCentered(TextFormat("Players ready: %d/%d", readyCount,
                                        u.team_count * u.team_size),
                             y, HINT_FONT_SIZE, LIGHTGRAY) +
                SECTION_GAP;
          }},
      lobbyUpdate);

  y = drawCentered(areReady ? "Press SPACE to cancel ready"
                            : "Press SPACE to ready up",
                   y, HINT_FONT_SIZE, LIGHTGRAY) +
      LINE_GAP;
  drawCentered("Press ESC to leave the lobby", y, HINT_FONT_SIZE, LIGHTGRAY);
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

                   entity_designer_.drawParticles(particles);
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

void GameDesigner::drawEndScreenBackdrop(float animTime) {
  DrawRectangle(0, 0, shared::SCREEN_WIDTH, shared::SCREEN_HEIGHT,
                Fade(BLACK, 0.6f));

  const auto pulse{(std::sin(animTime * 3.0f) + 1.0f) * 0.5f};
  const auto thickness{BORDER_MIN_THICKNESS +
                       pulse * (BORDER_MAX_THICKNESS - BORDER_MIN_THICKNESS)};

  DrawRectangleLinesEx({0, 0, shared::SCREEN_WIDTH, shared::SCREEN_HEIGHT},
                       thickness, Fade(GOLD, 0.3f + pulse * 0.5f));
}

void GameDesigner::drawEndScreen(const shared::GameState &state,
                                 Session *session, GameType gameType,
                                 float animTime) const {
  drawEndScreenBackdrop(animTime);

  std::array<shared::PlayerId, MAX_PLAYERS_ON_THIS_MACHINE> playerIds{};
  if (session)
    playerIds = session->getPlayersIds();

  const auto isOnThisMachine = [&playerIds](shared::PlayerId id) {
    return id != 0 &&
           std::find(playerIds.begin(), playerIds.end(), id) != playerIds.end();
  };

  // " (You)" only distinguishes anything online - in local play every
  // player on screen is on this machine.
  const auto marksYours = gameType == GameType::ONLINE;

  const auto scoreLine = [](const shared::PlayerState &p) {
    return std::to_string(p.points) + " pts" +
           (p.lives > 0 ? "  -  " + std::to_string(p.lives) +
                              (p.lives > 1 ? " lives left" : " life left")
                        : "  -  dead");
  };

  auto y{shared::SCREEN_HEIGHT / 2.0f - 220};

  std::visit(
      shared::overloaded{
          [&](const shared::CoopGameState &s) {
            using Phase = shared::CoopGameSim::Phase;
            const auto won{static_cast<Phase>(s.phase) == Phase::WON};

            y = drawCentered(won ? "VICTORY" : "GAME OVER", y, TITLE_FONT_SIZE,
                             won ? GOLD : RED) +
                LINE_GAP;
            y = drawCentered(won ? "The boss is down." : "The invasion won.", y,
                             HINT_FONT_SIZE, LIGHTGRAY) +
                SECTION_GAP;

            uint32_t total{};
            for (std::size_t i = 0; i < s.player_count; ++i)
              total += s.players[i].points;

            y = drawCentered(("Team score: " + std::to_string(total)).c_str(),
                             y, HEADING_FONT_SIZE, WHITE) +
                SECTION_GAP;

            for (std::size_t i = 0; i < s.player_count; ++i) {
              const auto &p = s.players[i];
              y = drawRosterRow(
                  playerLabel(p, marksYours && isOnThisMachine(p.id)),
                  scoreLine(p), p.lives > 0 ? LIGHTGRAY : RED, y);
            }

            y += LINE_GAP;
          },
          [&](const shared::PvPGameState &s) {
            using Outcome = shared::PvPGameSim::TeamOutcome;

            const auto teamsEnd{s.teams.begin() + s.teams_count};
            const auto ownsTeam = [&](const shared::TeamState &team) {
              return std::any_of(team.players.begin(),
                                 team.players.begin() + team.size,
                                 [&isOnThisMachine](const auto &p) {
                                   return isOnThisMachine(p.id);
                                 });
            };

            const auto myTeam{
                std::find_if(s.teams.begin(), teamsEnd, ownsTeam)};
            const auto myTeamsCount{
                std::count_if(s.teams.begin(), teamsEnd, ownsTeam)};

            // With both teams on this machine (local PvP) a "you won"
            // headline would be meaningless, so only personalise it when
            // exactly one team is ours.
            if (myTeamsCount == 1) {
              switch (static_cast<Outcome>(myTeam->outcome)) {
              case Outcome::WON:
                y = drawCentered("YOU WIN", y, TITLE_FONT_SIZE, GOLD);
                break;
              case Outcome::LOST:
                y = drawCentered("YOU LOSE", y, TITLE_FONT_SIZE, RED);
                break;
              default:
                y = drawCentered("DRAW", y, TITLE_FONT_SIZE, LIGHTGRAY);
                break;
              }
            } else {
              const auto winner{
                  std::find_if(s.teams.begin(), teamsEnd, [](const auto &team) {
                    return static_cast<Outcome>(team.outcome) == Outcome::WON;
                  })};

              if (winner == teamsEnd) {
                y = drawCentered("DRAW", y, TITLE_FONT_SIZE, LIGHTGRAY);
              } else {
                const auto winnerIdx{std::distance(s.teams.begin(), winner)};
                y = drawCentered(
                    ("TEAM " + std::to_string(winnerIdx + 1) + " WINS").c_str(),
                    y, TITLE_FONT_SIZE, TEAMS_COLORS[winnerIdx]);
              }
            }

            y += SECTION_GAP;

            for (std::size_t teamIdx = 0; teamIdx < s.teams_count; ++teamIdx) {
              const auto &team = s.teams[teamIdx];

              uint32_t total{};
              for (std::size_t i = 0; i < team.size; ++i)
                total += team.players[i].points;

              auto teamLabel{"Team " + std::to_string(teamIdx + 1)};
              if (myTeamsCount == 1 && ownsTeam(team))
                teamLabel += " (Yours)";

              switch (static_cast<Outcome>(team.outcome)) {
              case Outcome::WON:
                teamLabel += " - WON";
                break;
              case Outcome::LOST:
                teamLabel += " - LOST";
                break;
              case Outcome::DREW:
                teamLabel += " - DREW";
                break;
              default:
                break;
              }

              teamLabel += "  (" + std::to_string(total) + " pts)";

              y = drawCentered(teamLabel.c_str(), y, BODY_FONT_SIZE,
                               TEAMS_COLORS[teamIdx]) +
                  LINE_GAP;

              for (std::size_t i = 0; i < team.size; ++i) {
                const auto &p = team.players[i];
                y = drawRosterRow(
                    playerLabel(p, marksYours && isOnThisMachine(p.id)),
                    scoreLine(p), p.lives > 0 ? LIGHTGRAY : RED, y);
              }

              y += ROW_GAP;
            }
          }},
      state);

  y += LINE_GAP;
  y = drawCentered("Press R to play again", y, HINT_FONT_SIZE, LIGHTGRAY) +
      LINE_GAP;
  drawCentered("Press ENTER to return to the menu", y, HINT_FONT_SIZE,
               LIGHTGRAY);
}

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
