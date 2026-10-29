import { loadAssets } from "./scripts/assets";
import { appModule } from "./scripts/constants";
import { assertNeverForScreen, toScreen } from "./scripts/helpers";
import {
  endInputFrame,
  keysPressed,
  pollPlayerInputsInGame,
  setGameHasKeyboard,
} from "./scripts/inputs";
import type { CoopGameState, GameState } from "./scripts/messages";
import {
  type GameSession,
  LocalSession,
  OnlineSession,
  type OnlineSessionConfig,
} from "./scripts/session";
import type { Config, GameScreen, GameType } from "./scripts/types";
import { hideEndScreen, showEndScreen } from "./scripts/ui/end-screen";
import {
  clearScreen,
  drawCoopGame,
  drawPvPGame,
} from "./scripts/ui/game-renderer";
import { clearHud, updateHud } from "./scripts/ui/hud";
import { hideMenu, type MenuPage, showMenu } from "./scripts/ui/menu";
import { spawnEnemyExplosion, updateParticles } from "./scripts/ui/particles";
import { drawStars, updateStars } from "./scripts/ui/stars";
import "./styles/game-ui.css";
import "./styles/game.css";

const MAX_DELTA = 0.1;
let acc = 0;
let last = performance.now();

let screen: GameScreen;

const backToMenu = () => {
  screen = { kind: "menu" };
  clearHud();
  showMenu(mainPage);
};

const connectingToServerPage = (
  session: OnlineSession,
  maxPlayersCount: number,
): MenuPage => ({
  title: "Connecting to server...",
  hero: true,
  items: [],
  back: () => showMenu(namesPage(session.getConfig(), maxPlayersCount)),
});

const namesPage = (config: Config, maxPlayersCount: number): MenuPage => ({
  title: "Who is playing?",
  items: [],
  form: {
    fields: Array.from({
      length: config.playerCount,
    }).map((_, i) => ({
      name: `name${i + 1}`,
      label: `Player ${i + 1}`,
      placeholder: `Player ${i + 1}`,
      maxLength: appModule.MAX_USERNAME_LENGTH,
      allowed: /[A-Za-z_]/,
      required: true,
      rememberAs: `supergame.name${i + 1}`,
      autocomplete: "nickname",
    })),
    submitLabel: "Start",
    onSubmit: (values) =>
      startOnlineMatch({
        ...config,
        playerNames: Array.from({ length: config.playerCount }).map(
          (_, i) => values[`name${i + 1}`]!,
        ),
      }),
  },
  back: () =>
    showMenu(numberOfPlayersOnThisMachinePage(config, maxPlayersCount)),
});

const numberOfPlayersOnThisMachinePage = (
  config:
    | Omit<Config & { kind: "coop" }, "playerCount">
    | Omit<Config & { kind: "pvp" }, "playerCount">,
  maxPlayerCount: number,
): MenuPage => ({
  title: "Choose the number of players from this machine",
  items: Array.from({
    length: Math.min(maxPlayerCount, appModule.MAX_PLAYERS_PER_CLIENT),
  }).map((_, i) => ({
    label: `${i + 1} player${i > 0 ? "s" : ""}`,
    action: () =>
      showMenu(namesPage({ ...config, playerCount: i + 1 }, maxPlayerCount)),
  })),
  back: () =>
    config.kind === "coop"
      ? showMenu(modePage("online"))
      : showMenu(pvpPage("online")),
});

const pausePage = (
  currentScreen: GameScreen & { kind: "pause" },
): MenuPage => ({
  title: "Game paused",
  items: [
    {
      label: "Resume",
      action: () => {
        screen = { kind: "playing", match: currentScreen.match };
        hideMenu();
      },
    },
    {
      label: "Restart",
      action: () =>
        restart(currentScreen.match.config, currentScreen.match.session),
    },
    {
      label: "Quit to menu",
      action: backToMenu,
    },
  ],
  back: () => {
    screen = { kind: "playing", match: currentScreen.match };
    hideMenu();
  },
  doNotShowBackButton: true,
});

const coopPage: MenuPage = {
  title: "Fighting the invasion",
  items: Array.from({
    length: Math.min(
      appModule.MAX_PLAYERS_COOP,
      appModule.MAX_PLAYERS_ON_THIS_MACHINE,
    ),
  }).map((_, i) => ({
    label: `${i + 1} player${i > 0 ? "s" : ""}`,
    action: () =>
      startLocalMatch({
        kind: "coop",
        playerCount: i + 1,
      }),
  })),
  back: () => showMenu(mainPage),
};

const pvpPage = (gameType: GameType): MenuPage => {
  const buildMatchUps = (): [number, number][] => {
    const matchUps: [number, number][] = [];

    for (let teamCount = 2; teamCount <= appModule.MAX_TEAMS; ++teamCount) {
      const seats =
        gameType === "online"
          ? appModule.MAX_PLAYERS_PER_TEAM
          : Math.min(
              Math.floor(appModule.MAX_PLAYERS_ON_THIS_MACHINE / teamCount),
              appModule.MAX_PLAYERS_PER_TEAM,
            );

      for (let playerCount = 1; playerCount <= seats; ++playerCount) {
        matchUps.push([teamCount, playerCount]);
      }
    }

    return matchUps;
  };

  return {
    title: "Fighting other players",
    items: buildMatchUps().map((m) => ({
      label: `${m[0]} teams of ${m[1]} player${m[1] > 1 ? "s" : ""}`,
      action: () => {
        const config: Omit<Config & { kind: "pvp" }, "playerCount"> = {
          kind: "pvp",
          teamCount: m[0],
          teamSize: m[1],
        };

        if (gameType === "online") {
          showMenu(numberOfPlayersOnThisMachinePage(config, m[1]));
        } else {
          startLocalMatch({
            ...config,
            playerCount: m[0] * m[1],
          });
        }
      },
    })),
    back: () => showMenu(mainPage),
  };
};

const modePage = (gameType: GameType): MenuPage => ({
  title: "Select mode",
  items: [
    {
      label: "Single/Coop against enemies",
      action: () => {
        if (gameType === "local") showMenu(coopPage);
        else
          showMenu(
            numberOfPlayersOnThisMachinePage(
              {
                kind: "coop",
              },
              Math.min(
                appModule.MAX_PLAYERS_COOP,
                appModule.MAX_PLAYERS_ON_THIS_MACHINE,
              ),
            ),
          );
      },
    },
    {
      label: "PvP against other players",
      action: () => showMenu(pvpPage(gameType)),
    },
  ],
  back: () => showMenu(mainPage),
});

const mainPage: MenuPage = {
  title: "SUPER GAME",
  hero: true,
  items: [
    {
      label: "Play local",
      action: () => showMenu(modePage("local")),
    },
    {
      label: "Play online",
      action: () => showMenu(modePage("online")),
    },
  ],
};

function step(now: number) {
  handleInputs();

  acc += Math.min((now - last) / 1000, MAX_DELTA);
  last = now;

  while (acc >= appModule.FIXED_DT) {
    update();
    updateStars(appModule.FIXED_DT);
    updateParticles(appModule.FIXED_DT);
    acc -= appModule.FIXED_DT;
  }

  drawStars();
  draw();
  endInputFrame();
  requestAnimationFrame(step);
}

function init() {
  screen = { kind: "menu" };
  showMenu(mainPage);
}

function restart(config: Config, session: GameSession) {
  hideEndScreen();
  clearHud();

  if (session instanceof LocalSession) {
    startLocalMatch(config);
  } else if (session instanceof OnlineSession) {
    startOnlineMatch(session.getConfig());
  }
}

function draw() {
  document.getElementById("layout")!.dataset.screen = screen.kind;

  clearScreen();

  if (screen.kind === "playing" || screen.kind === "pause") {
    const { state: gs, session } = screen.match;

    if (!gs) return;

    const myIds = session.getPlayerIds();

    switch (gs.mode) {
      case "coop":
        drawCoopGame(gs.state, myIds);
        break;

      case "pvp":
        drawPvPGame(
          gs.state,
          myIds,
          session instanceof LocalSession ? "local" : "online",
        );
        break;
    }

    updateHud(gs, myIds);
  }
}

function handleInputs() {
  switch (screen.kind) {
    case "playing":
      if (keysPressed.has("Escape")) {
        screen = {
          kind: "pause",
          match: screen.match,
        };
        showMenu(pausePage(screen));
      }
      break;

    case "pause":
    case "menu":
    case "end":
      break;

    default:
      return assertNeverForScreen(screen);
  }
}

function update() {
  setGameHasKeyboard(false);

  switch (screen.kind) {
    case "playing": {
      const { session, config } = screen.match;
      setGameHasKeyboard(true);
      const state = session.step(
        pollPlayerInputsInGame(session.getPlayerIds()),
      );

      screen.match.state = state;

      executeEventsOnStateChange();

      if (isOver(screen.match.state)) {
        screen = {
          kind: "end",
          match: screen.match,
        };

        showEndScreen(state, session.getPlayerIds(), (action) => {
          switch (action) {
            case "restart":
              restart(config, session);
              break;

            case "menu":
              hideEndScreen();
              backToMenu();
              break;
          }
        });

        break;
      }

      screen.match.prevState = screen.match.state;

      break;
    }

    case "menu":
    case "pause":
    case "end":
      break;

    default:
      return assertNeverForScreen(screen);
  }
}

function isOver(s: GameState) {
  switch (s.mode) {
    case "coop":
      return (
        s.state.phase === appModule.CoopGamePhase.WON ||
        s.state.phase === appModule.CoopGamePhase.GAME_OVER
      );

    case "pvp":
      return s.state.phase === appModule.PvPGamePhase.END;
  }
}

function startLocalMatch(config: Config) {
  screen = {
    kind: "playing",
    match: {
      config,
      session: new LocalSession(config),
      state: null,
      prevState: null,
    },
  };
  hideMenu();
}

function startOnlineMatch(
  config:
    | Omit<OnlineSessionConfig & { kind: "coop" }, "ready" | "serverUrl">
    | Omit<OnlineSessionConfig & { kind: "pvp" }, "ready" | "serverUrl">,
) {
  const onlineConfig: OnlineSessionConfig = {
    ...config,
    ready: false,
    serverUrl: new URL(import.meta.env.VITE_GAME_SERVER_URL),
  };

  for (let i = 0; i < config.playerCount; i++) {
    if (i >= config.playerNames.length) break;
    onlineConfig.serverUrl.searchParams.append(
      `name${i + 1}`,
      config.playerNames[i]!,
    );
  }

  onlineConfig.serverUrl.searchParams.append(
    "slots",
    onlineConfig.playerCount.toString(),
  );

  if (onlineConfig.kind === "pvp") {
    onlineConfig.serverUrl.searchParams.append(
      "team_count",
      onlineConfig.teamCount.toString(),
    );
    onlineConfig.serverUrl.searchParams.append(
      "team_size",
      onlineConfig.teamSize.toString(),
    );
  }

  showMenu(
    connectingToServerPage(new OnlineSession(onlineConfig), config.playerCount),
  );
}

function executeEventsOnStateChange() {
  if (
    screen.kind !== "playing" ||
    !screen.match.state ||
    !screen.match.prevState
  )
    return;

  switch (screen.match.state.mode) {
    case "coop": {
      const state = screen.match.state.state;
      const prevState = screen.match.prevState.state as CoopGameState;

      spawnEnemiesExplosions(prevState, state);

      break;
    }
  }
}

function spawnEnemiesExplosions(before: CoopGameState, after: CoopGameState) {
  for (let i = 0; i < after.enemies.length; ++i) {
    const wasAlive = before.enemies[i]?.[0] === 1;
    const isAlive = after.enemies[i]?.[0] === 1;

    if (wasAlive && !isAlive) {
      const x =
        after.enemies_offset_x +
        (i % appModule.SIM_ENEMIES_POOL_COLS) *
          (appModule.SIM_ENEMY_WIDTH + appModule.SIM_ENEMIES_POOL_SPACING_X);

      const y =
        after.enemies_offset_y -
        Math.floor(i / appModule.SIM_ENEMIES_POOL_COLS) *
          (appModule.SIM_ENEMY_HEIGHT + appModule.SIM_ENEMIES_POOL_SPACING_Y);

      spawnEnemyExplosion(toScreen({ x, y }), after.enemies[i]![1]);
    }
  }
}

await loadAssets();
init();
requestAnimationFrame(step);
