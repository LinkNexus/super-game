import { loadAssets } from "./scripts/assets";
import { loadAudio, playSfx, setMusicVolume } from "./scripts/audio";
import { appModule } from "./scripts/constants";
import {
  assertNeverForScreen,
  enterFullscreenOnTouch,
  isTouchOnly,
  toScreen,
} from "./scripts/helpers";
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
import { hideLobby, showLobby } from "./scripts/ui/lobby";
import {
  hideMenu,
  type MenuItem,
  type MenuPage,
  showMenu,
} from "./scripts/ui/menu";
import { spawnEnemyExplosion, updateParticles } from "./scripts/ui/particles";
import { drawStars, updateStars } from "./scripts/ui/stars";
import { showToast } from "./scripts/ui/toast";
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
      startOnlineMatch(
        {
          ...config,
          playerNames: Array.from({ length: config.playerCount }).map(
            (_, i) => values[`name${i + 1}`]!,
          ),
        },
        maxPlayersCount,
      ),
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
    length: Math.min(
      maxPlayerCount,
      isTouchOnly() ? 1 : appModule.MAX_PLAYERS_PER_CLIENT,
    ),
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
      action: () => restart(currentScreen.match.session),
    },
    {
      label: "Quit to menu",
      action: () => {
        if (currentScreen.match.session instanceof OnlineSession) {
          currentScreen.match.session.end();
          return;
        }
        backToMenu();
      },
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
    length: isTouchOnly()
      ? 1
      : Math.min(
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
  items: () => {
    const commonMenu: MenuItem = {
      label: "Single/Coop against enemies",
      action: () => {
        if (gameType === "local") showMenu(coopPage);
        else
          showMenu(
            numberOfPlayersOnThisMachinePage(
              { kind: "coop" },
              isTouchOnly()
                ? 1
                : Math.min(
                    appModule.MAX_PLAYERS_PER_CLIENT,
                    appModule.MAX_PLAYERS_ON_THIS_MACHINE,
                  ),
            ),
          );
      },
    };

    return gameType === "local" && isTouchOnly()
      ? [commonMenu]
      : [
          commonMenu,
          {
            label: "PvP against other players",
            action: () => showMenu(pvpPage(gameType)),
          },
        ];
  },
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

  setMusicVolume(screen.kind === "playing" ? 0.25 : 0.7);
  drawStars();
  draw();
  endInputFrame();
  requestAnimationFrame(step);
}

function init() {
  screen = { kind: "menu" };
  showMenu(mainPage);
}

function restart(session: GameSession) {
  hideEndScreen();
  clearHud();

  if (session instanceof LocalSession) {
    startLocalMatch(session.getConfig());
  } else if (session instanceof OnlineSession) {
    session.end(false);
    const config = session.getConfig();
    const maxPlayersCount =
      config.kind === "pvp"
        ? config.teamSize
        : isTouchOnly()
          ? 1
          : Math.min(
              appModule.MAX_PLAYERS_PER_CLIENT,
              appModule.MAX_PLAYERS_ON_THIS_MACHINE,
            );
    startOnlineMatch(config, maxPlayersCount);
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

    case "lobby":
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
      const { session } = screen.match;
      setGameHasKeyboard(true);
      const state = session.step(
        pollPlayerInputsInGame(session.getPlayerIds()),
      );

      if (!state) break;

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
              restart(session);
              break;

            case "menu":
              hideEndScreen();
              session instanceof OnlineSession && session.end(false);
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
    case "lobby":
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
  enterFullscreenOnTouch();
  screen = {
    kind: "playing",
    match: {
      session: new LocalSession(config),
      state: null,
      prevState: null,
    },
  };
  hideMenu();
}

function startOnlineMatch(
  config: OnlineSessionConfig,
  maxPlayersCount: number,
) {
  enterFullscreenOnTouch();
  const dismiss = showToast("Connecting to the server...", { durationMs: 0 });

  new OnlineSession({
    config,
    onLobbyUpdateReceived: (currentSession, u) => {
      hideMenu();
      dismiss();

      if (u.update.game_started) {
        hideLobby();
        screen = {
          kind: "playing",
          match: {
            session: currentSession,
            state: null,
            prevState: null,
          },
        };
        return;
      }

      screen = {
        kind: "lobby",
        session: currentSession,
      };
      showLobby(u, currentSession.getPlayerIds(), {
        onReady: (id, r) => currentSession.sendReady(r, id),
        onSwitchTeam: (id, t) => currentSession.switchTeam(id, t),
        onLeave: () => {
          currentSession.leaveLobby();
        },
      });
    },
    onConnectionError: () => {
      dismiss();
      showMenu(namesPage(config, maxPlayersCount));
    },
    onConnectionClosed: ({ leaving, returnBackToMenu }) => {
      dismiss();
      hideLobby();
      hideEndScreen();

      if (typeof returnBackToMenu === "boolean") {
        if (returnBackToMenu) backToMenu();
        else showMenu(namesPage(config, maxPlayersCount));
      }

      if (!leaving)
        showToast("A problem occured with the connection to the server", {
          title: "Connection Error",
          kind: "error",
        });
    },
  });
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

      if (
        prevState.boss.active &&
        state.boss.active &&
        prevState.boss.health > state.boss.health
      )
        playSfx("boss_hit");

      break;
    }
  }
}

function spawnEnemiesExplosions(before: CoopGameState, after: CoopGameState) {
  for (let i = 0; i < after.enemies.length; ++i) {
    const wasAlive = before.enemies[i]?.[0];
    const isAlive = after.enemies[i]?.[0];

    if (wasAlive && !isAlive) {
      const x =
        after.enemies_offset.x +
        (i % appModule.SIM_ENEMY_COLS) *
          (appModule.SIM_ENEMY_WIDTH + appModule.SIM_ENEMIES_POOL_SPACING_X);

      const y =
        after.enemies_offset.y -
        Math.floor(i / appModule.SIM_ENEMY_COLS) *
          (appModule.SIM_ENEMY_HEIGHT + appModule.SIM_ENEMIES_POOL_SPACING_Y);

      playSfx("explosion");
      spawnEnemyExplosion(toScreen({ x, y }), after.enemies[i]![1]);
    }
  }
}

document
  .getElementById("layout")
  ?.addEventListener("contextmenu", (e) => e.preventDefault());
await Promise.all([loadAssets(), loadAudio()]);
init();
requestAnimationFrame(step);
