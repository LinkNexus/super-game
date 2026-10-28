import type { GAME_TYPES } from "./constants";
import type {
  CoopGameState,
  GameState,
  GameStateMessage,
  PvPGameState,
} from "./messages";
import type { GameSession } from "./session";

export type Config =
  | {
      kind: "coop";
      playerCount: number;
    }
  | {
      kind: "pvp";
      matchUps: [number, number][];
      selectedMatchUpIdx: number;
    };

type Match = {
  session: GameSession;
  config: Config;
  state: GameState | null;
  prevState: GameState | null;
};

export type GameType = (typeof GAME_TYPES)[number];

export type GameScreen =
  | {
      kind: "menu";
      selectedGameType: GameType;
    }
  | { kind: "playing"; match: Match }
  | {
      kind: "playMenu";
      match: Match;
    }
  | {
      kind: "end";
      match: Omit<Match, "prevState">;
    };

// export type GameScreen =
//   | "menu"
//   | "selectMode"
//   | "selectPlayerCount"
//   | "selectPlayerCountOnThisMachine"
//   | "enterPlayerNames"
//   | "connectingToServer"
//   | "lobby"
//   | "playing"
//   | "paused"
//   | "end";
