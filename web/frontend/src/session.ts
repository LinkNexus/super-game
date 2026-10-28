import { sim } from "./constants";
import type { GameState, PlayerIds, PlayerInputs } from "./messages";
import type { Config } from "./types";

export abstract class GameSession {
  abstract getPlayerIds(): PlayerIds;
  abstract step(inputs: PlayerInputs): GameState;
}

export class LocalSession extends GameSession {
  private gameType: Config["kind"];

  constructor(config: Config) {
    super();

    this.gameType = config.kind;

    switch (config.kind) {
      case "coop":
        sim.startCoop(config.playerCount);
        break;

      case "pvp": {
        const [teamCount, teamSize] =
          config.matchUps[config.selectedMatchUpIdx]!;
        sim.startPvP(teamCount, teamSize);
        break;
      }
    }
  }

  getPlayerIds(): PlayerIds {
    return JSON.parse(sim.getPlayerIds());
  }

  step(inputs: PlayerInputs): GameState {
    return {
      mode: this.gameType,
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
  constructor(config: OnlineSessionConfig) {
    super();
  }

  getPlayerIds(): PlayerIds {
    throw new Error("Method not implemented.");
  }

  step(inputs: PlayerInputs): GameState {
    throw new Error("Method not implemented.");
  }
}
