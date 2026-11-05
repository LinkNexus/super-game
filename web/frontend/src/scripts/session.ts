import { appModule, sim } from "./constants";
import type {
  ClientMessage,
  CoopGameState,
  GameState,
  LobbyUpdate,
  PlayerId,
  PlayerIds,
  PlayerInputs,
  PvPGameState,
  ServerMessage,
  Vec2,
} from "./messages";
import type { Config } from "./types";

export abstract class GameSession {
  abstract getPlayerIds(): PlayerIds;
  abstract step(inputs: PlayerInputs): GameState | null;
}

export class LocalSession extends GameSession {
  private config: Config;

  constructor(config: Config) {
    super();

    this.config = config;

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
      mode: this.config.kind,
      state: JSON.parse(sim.step(JSON.stringify(inputs))),
    };
  }

  getConfig() {
    return this.config;
  }
}

export type OnlineSessionConfig = Config & {
  playerNames: string[];
};

type ConnectionErrorHandler = (error: Event) => void;
type LobbyUpdateHandler = (
  currentSession: OnlineSession,
  update: LobbyUpdate,
) => void;

export class OnlineSession extends GameSession {
  private config: OnlineSessionConfig;
  private ws: WebSocket;
  private leaving: boolean = false;
  private returnBackToMenu: boolean | undefined = false;
  private myPlayersIds: PlayerIds = [];

  private prevState: GameState | null = null;
  private targetState: GameState | null = null;
  private currentState: GameState | null = null;

  private lastStepTime: number = 0;

  buildServerUrl(): URL {
    const serverUrl = new URL(import.meta.env.VITE_GAME_SERVER_URL);

    for (let i = 0; i < this.config.playerCount; ++i) {
      if (i >= this.config.playerNames.length) break;
      serverUrl.searchParams.append(
        `name${i + 1}`,
        this.config.playerNames[i]!,
      );
    }

    serverUrl.searchParams.append("slots", this.config.playerCount.toString());

    if (this.config.kind === "pvp") {
      serverUrl.searchParams.append(
        "team_count",
        this.config.teamCount.toString(),
      );
      serverUrl.searchParams.append(
        "team_size",
        this.config.teamSize.toString(),
      );
    }

    return serverUrl;
  }

  constructor({
    config,
    onLobbyUpdateReceived,
    onConnectionError,
    onConnectionClosed,
  }: {
    config: OnlineSessionConfig;
    onLobbyUpdateReceived: LobbyUpdateHandler;
    onConnectionError?: ConnectionErrorHandler;
    onConnectionClosed?: ({
      event,
      leaving,
      returnBackToMenu,
    }: {
      event: CloseEvent;
      leaving: boolean;
      returnBackToMenu: boolean | undefined;
    }) => void;
  }) {
    super();

    this.config = config;
    this.ws = new WebSocket(this.buildServerUrl());

    this.ws.addEventListener("open", (e) => {
      console.log("Connected to server", e);
    });
    this.ws.addEventListener("error", (e) => onConnectionError?.(e));
    this.ws.addEventListener("close", (e) => {
      onConnectionClosed?.({
        event: e,
        leaving: this.leaving,
        returnBackToMenu: this.returnBackToMenu,
      });
      this.leaving = false;
      this.returnBackToMenu = false;
    });
    this.ws.addEventListener("message", (e) =>
      this.onWsMessage({
        event: e,
        onLobbyUpdateReceived,
      }),
    );
  }

  getConfig() {
    return this.config;
  }

  leaveLobby() {
    this.leaving = true;
    this.ws.close();
  }

  end(redirect: boolean = true) {
    this.returnBackToMenu = redirect ?? "undefined";
    this.leaving = true;
    this.ws.close();
  }

  sendReady(ready: boolean, playerId: PlayerId) {
    this.ws.send(
      JSON.stringify({
        type: appModule.ClientMessageType.READY,
        payload: {
          is_ready: ready,
          player_id: playerId,
        },
      } satisfies ClientMessage),
    );
  }

  switchTeam(playerId: PlayerId, targetTeamId: number) {
    this.ws.send(
      JSON.stringify({
        type: appModule.ClientMessageType.SWITCH_TEAM,
        payload: {
          player_id: playerId,
          target_team_id: targetTeamId,
        },
      } satisfies ClientMessage),
    );
  }

  onWsMessage({
    event,
    onLobbyUpdateReceived,
  }: {
    event: MessageEvent;
    onLobbyUpdateReceived: LobbyUpdateHandler;
  }) {
    if (typeof event.data !== "string") return;

    const msg = JSON.parse(event.data) as ServerMessage;

    switch (msg.type) {
      case appModule.ServerMessageType.WELCOME: {
        this.myPlayersIds = msg.payload.players_ids;
        break;
      }

      case appModule.ServerMessageType.COOP_GAME_LOBBY_UPDATE: {
        onLobbyUpdateReceived(this, {
          mode: "coop",
          update: msg.payload,
        });
        break;
      }

      case appModule.ServerMessageType.PVP_GAME_LOBBY_UPDATE: {
        onLobbyUpdateReceived(this, {
          mode: "pvp",
          update: msg.payload,
        });
        break;
      }

      case appModule.ServerMessageType.COOP_GAME_STATE: {
        this.currentState = {
          mode: "coop",
          state: msg.payload,
        };
        break;
      }

      case appModule.ServerMessageType.PVP_GAME_STATE: {
        this.currentState = {
          mode: "pvp",
          state: msg.payload,
        };
        break;
      }
    }
  }

  getPlayerIds(): PlayerIds {
    return this.myPlayersIds;
  }

  step(inputs: PlayerInputs): GameState | null {
    if (this.ws.readyState === WebSocket.OPEN) {
      this.ws.send(
        JSON.stringify({
          type: appModule.ClientMessageType.PLAYER_INPUT,
          payload: Array.from({
            length: appModule.MAX_PLAYERS_PER_CLIENT,
          }).map((_, i) => {
            if (i >= inputs.length) return null;
            return inputs[i]!;
          }),
        } satisfies ClientMessage),
      );
    }

    if (this.currentState) {
      this.prevState = this.targetState;
      this.targetState = this.currentState;
      this.currentState = null;
      this.lastStepTime = performance.now();
    }

    return this.interpolateStates();
  }

  private interpolateStates(): GameState | null {
    if (!this.targetState) return null;
    if (!this.prevState) return this.targetState;

    const alpha = Math.min(
      (performance.now() - this.lastStepTime) / (1000 * appModule.FIXED_DT),
      1,
    );

    const lerpVec2 = (prev: Vec2, target: Vec2): Vec2 => ({
      x: prev.x + (target.x - prev.x) * alpha,
      y: prev.y + (target.y - prev.y) * alpha,
    });

    const lerpBullets = <S extends { bullets: GameState["state"]["bullets"] }>(
      prev: S,
      target: S,
    ): S["bullets"] => {
      const interBullets: S["bullets"] = [];

      for (let i = 0; i < target.bullets.length; ++i) {
        const targetB = target.bullets[i]!;
        const interB = {
          ...targetB,
          position: { ...targetB.position },
        };

        if (i < prev.bullets.length) {
          const prevBullet = prev.bullets[i]!;
          if (prevBullet.active && targetB.active)
            interB.position = lerpVec2(prevBullet.position, targetB.position);
        }

        interBullets.push(interB);
      }

      return interBullets;
    };
    const findEntityById = <T extends { id: number }>(
      entities: (T | null)[],
      id: number,
    ): T | null => {
      return entities.find((e) => e && e.id === id) || null;
    };
    const lerpPlayers = <S extends { players: CoopGameState["players"] }>(
      prev: S,
      target: S,
    ): S["players"] => {
      return target.players.map((targetP) => {
        if (!targetP) return null;

        const interP = {
          ...targetP,
          position: { ...targetP.position },
        };
        const prevP = findEntityById(prev.players, targetP.id);

        if (prevP) interP.position = lerpVec2(prevP.position, targetP.position);

        return interP;
      });
    };

    const lerpCoop = (
      prev: CoopGameState,
      target: CoopGameState,
    ): CoopGameState => {
      return {
        ...target,
        bullets: lerpBullets(prev, target),
        players: lerpPlayers(prev, target),
        boss: {
          ...target.boss,
          position: lerpVec2(prev.boss.position, target.boss.position),
        },
        enemies: target.enemies.map((slot) => [...slot]),
        enemies_offset: lerpVec2(prev.enemies_offset, target.enemies_offset),
      };
    };

    const lerpPvP = (prev: PvPGameState, target: PvPGameState) => {
      return {
        ...target,
        bullets: lerpBullets(prev, target),
        teams: target.teams.map((t) => {
          if (!t) return null;

          const prevTeam = findEntityById(prev.teams, t.id);
          return {
            ...t,
            players: prevTeam ? lerpPlayers(prevTeam, t) : t.players,
          };
        }),
      };
    };

    if (this.prevState.mode === "coop" && this.targetState.mode === "coop")
      return {
        mode: "coop",
        state: lerpCoop(this.prevState.state, this.targetState.state),
      };
    else if (this.prevState.mode === "pvp" && this.targetState.mode === "pvp")
      return {
        mode: "pvp",
        state: lerpPvP(this.prevState.state, this.targetState.state),
      };

    return this.targetState;
  }
}
