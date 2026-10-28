import { loadAssets } from "./assets";
import { appModule } from "./constants";
import { draw } from "./game-renderer";
import { assertNeverForScreen, toScreen } from "./helpers";
import { endInputFrame, keysPressed, pollPlayerInputsInGame } from "./inputs";
import type { CoopGameState, GameState } from "./messages";
import { spawnEnemyExplosion, updateParticles } from "./particles";
import { LocalSession } from "./session";
import { drawStars, updateStars } from "./stars";
import type { Config, GameScreen } from "./types";

const MAX_DELTA = 0.1;
let acc = 0;
let last = performance.now();

let screen: GameScreen;

function step(now: number) {
  handleInputs();

  acc += Math.min((now - last) / 1000, MAX_DELTA);
  last = now;

  while (acc >= appModule.FIXED_DT) {
    update();
    updateStars(appModule.FIXED_DT);
    acc -= appModule.FIXED_DT;
  }

  updateParticles(appModule.FIXED_DT);

  drawStars();
  draw(screen);
  endInputFrame();
  requestAnimationFrame(step);
}

function init() {
  const config: Config = {
    kind: "coop",
    playerCount: 1,
  };

  screen = {
    kind: "playing",
    match: {
      config,
      session: new LocalSession(config),
      state: null,
      prevState: null,
    },
  };
}

function restart() {
  // if (screen.match.session instanceof LocalSession) {
  //   startLocalSession();
  // } else {
  //   startOnlineSession();
  // }
}

function handleInputs() {
  switch (screen.kind) {
    case "playing":
      if (keysPressed.has("Escape")) {
        screen = {
          kind: "playMenu",
          match: screen.match,
        };
      }
      break;

    case "playMenu":
      if (keysPressed.has("Escape")) {
        screen = {
          kind: "playing",
          match: screen.match,
        };
      }
      break;

    case "menu":
    case "end":
      break;

    default:
      return assertNeverForScreen(screen);
  }
}

function update() {
  switch (screen.kind) {
    case "playing": {
      const { session } = screen.match;
      screen.match.state = session.step(
        pollPlayerInputsInGame(session.getPlayerIds()),
      );

      executeEventsOnStateChange();

      if (isOver(screen.match.state)) {
        console.log(screen.match.state);
        screen = {
          kind: "end",
          match: screen.match,
        };
        break;
      }

      screen.match.prevState = screen.match.state;

      break;
    }

    case "menu":
    case "playMenu":
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

// function startLocalSession() {
//   screen = {
//     kind: "playing",
// match: {
//       config: screen.match.config,
//       session: new LocalSession(screen.match.config),
//     }
//   }
//   configuration.session = new LocalSession(configuration.config);
//   screen = "playing";
// }
//
// function startOnlineSession() {
//   if (!configuration?.config) return;
//
//   if (configuration.type !== "online") return;
//
//   const { config } = configuration;
//   config.serverUrl = new URL(import.meta.env.VITE_GAME_SERVER_URL);
//
//   for (let i = 0; i < config.playerCount; i++) {
//     if (i >= config.playerNames.length) break;
//     config.serverUrl.searchParams.append(
//       `name${i + 1}`,
//       config.playerNames[i]!,
//     );
//   }
//
//   config.serverUrl.searchParams.append("slots", config.playerCount.toString());
//
//   switch (config.modeIdx) {
//     case 1: {
//       const [teamCount, teamSize] = config.mode.value.matchUps;
//
//       config.serverUrl.searchParams.append("team_count", teamCount!.toString());
//       config.serverUrl.searchParams.append("team_size", teamSize!.toString());
//
//       break;
//     }
//   }
//
//   configuration.session = new OnlineSession(config);
//   config.ready = false;
//   config.state = null;
//   config.prevState = null;
// }

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
