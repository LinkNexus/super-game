import type { GAME_TYPES } from "./constants";
import type { GameState } from "./messages";
import type { GameSession } from "./session";

export type Config = { playerCount: number } & (
  | {
      kind: "coop";
    }
  | {
      kind: "pvp";
      teamSize: number;
      teamCount: number;
    }
);

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
    }
  | { kind: "playing"; match: Match }
  | {
      kind: "pause";
      match: Match;
    }
  | {
      kind: "end";
      match: Omit<Match, "prevState">;
    };
