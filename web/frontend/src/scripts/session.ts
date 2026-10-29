import { sim } from "./constants";
import type { GameState, PlayerIds, PlayerInputs } from "./messages";
import type { Config } from "./types";

export abstract class GameSession {
  abstract getPlayerIds(): PlayerIds;
  abstract step(inputs: PlayerInputs): GameState;
}

export class LocalSession extends GameSession {
  private gameMode: Config["kind"];

  constructor(config: Config) {
    super();

    this.gameMode = config.kind;

    switch (config.kind) {
      case "coop":
        sim.startCoop(config.playerCount);
        break;

      case "pvp": {
        sim.startPvP(config.teamCount, config.teamSize);
        break;
      }
    }
  }

  getPlayerIds(): PlayerIds {
    return JSON.parse(sim.getPlayerIds());
  }

  step(inputs: PlayerInputs): GameState {
    return {
      mode: this.gameMode,
      state: JSON.parse(sim.step(JSON.stringify(inputs))),
    };
  }
}

export type OnlineSessionConfig = Config & {
  playerNames: string[];
  ready: boolean;
  serverUrl: URL;
};

export class OnlineSession extends GameSession {
  private config: OnlineSessionConfig;

  constructor(config: OnlineSessionConfig) {
    super();

    this.config = config;
  }

  getPlayerIds(): PlayerIds {
    throw new Error("Method not implemented.");
  }

  step(_inputs: PlayerInputs): GameState {
    throw new Error("Method not implemented.");
  }

  getConfig() {
    return this.config;
  }
}
