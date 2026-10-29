// TS mirror of shared/include/shared/messages.h: the JSON that WebSim.step()
// returns and that the server sends/receives over the WebSocket. Field names
// and nesting must match the NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE lists there.
// std::optional<T> is `T | null`; std::array<T, N> is a plain `T[]` of
// length N.
//
// Enum numbers that Embind binds (web/wasm/src/web_sim.cpp) are taken from
// the generated .d.mts, so they can't drift; the rest are mirrored by hand
// below, each with its C++ source.
import type { MainModule } from "@wasm/supergame-web-sim.mjs";

type Bound = MainModule;

export type ServerMessageType = Bound["ServerMessageType"];
export type ClientMessageType = Bound["ClientMessageType"];
export type CoopGamePhase = Bound["CoopGamePhase"][keyof Bound["CoopGamePhase"]];
export type PvPGamePhase = Bound["PvPGamePhase"][keyof Bound["PvPGamePhase"]];

/// shared::Button (messages.h), a bitmask: OR them together.
export const Button = {
  NONE: 0,
  LEFT: 1 << 0,
  RIGHT: 1 << 1,
  SHOOT: 1 << 2,
} as const;

/// shared::BulletType (sim/bullet_sim.h).
export const BulletType = { ENEMY: 0, PLAYER: 1 } as const;
export type BulletType = (typeof BulletType)[keyof typeof BulletType];

/// shared::EnemyType (sim/enemy_sim.h).
export const EnemyType = { TYPE_1: 0, TYPE_2: 1 } as const;
export type EnemyType = (typeof EnemyType)[keyof typeof EnemyType];

/// PvPGameSim::TeamOutcome (sim/game_sim.h), sent as TeamState.outcome.
export const TeamOutcome = { PLAYING: 0, WON: 1, LOST: 2, DREW: 3 } as const;
export type TeamOutcome = (typeof TeamOutcome)[keyof typeof TeamOutcome];

export type PlayerId = number;
/// PlayerIds<N>: N slots, null where no player.
export type PlayerIds = (PlayerId | null)[];

/// shared::Vec2D (math_utils.h).
export interface Vec2D {
  x: number;
  y: number;
}

// --- Client -> server --------------------------------------------------

export interface PlayerInput {
  buttons: number;
  player_id: PlayerId;
}

/// PlayerInputs<N>: always N slots, null-padded (WebSim.step() and the
/// server both read a fixed-size std::array).
export type PlayerInputs = (PlayerInput | null)[];

export interface ReadyMessage {
  is_ready: boolean;
}

// --- Server -> client: lobby -------------------------------------------

export interface WelcomeMessage {
  players_ids: PlayerIds;
}

export interface PlayerInfo {
  id: PlayerId;
  name: string;
  is_ready: boolean;
}

export interface CoopGameLobbyUpdate {
  players: (PlayerInfo | null)[];
  game_started: boolean;
}

export interface PvPGameLobbyUpdate {
  teams: ((PlayerInfo | null)[] | null)[];
  team_size: number;
  game_started: boolean;
}

// --- Server -> client: game state --------------------------------------

export interface PlayerState {
  position: Vec2D;
  lives: number;
  points: number;
  orientation: number;
  name: string;
  id: PlayerId;
}

export interface BulletState {
  position: Vec2D;
  type: BulletType;
  /// 0/1 (uint8_t on the wire): whether this pool slot is in flight.
  active: number;
}

export interface BossState {
  position: Vec2D;
  /// 0/1 (uint8_t on the wire).
  active: number;
  health: number;
  max_health: number;
}

/// One EnemiesPoolState slot: [alive (0/1), EnemyType], COLS * MAX_ROWS of them.
export type EnemySlot = [alive: number, type: EnemyType];

export interface CoopGameState {
  phase: CoopGamePhase;
  bullets: BulletState[];
  players: (PlayerState | null)[];
  enemies: EnemySlot[];
  enemies_offset_x: number;
  enemies_offset_y: number;
  boss: BossState;
}

export interface TeamState {
  id: number;
  outcome: TeamOutcome;
  players: (PlayerState | null)[];
}

export interface PvPGameState {
  phase: PvPGamePhase;
  bullets: BulletState[];
  teams: (TeamState | null)[];
}

// --- Envelopes ({type, payload}) ---------------------------------------

type Envelope<T, P> = { type: T; payload: P };

export type ServerMessage =
  | Envelope<ServerMessageType["WELCOME"], WelcomeMessage>
  | Envelope<ServerMessageType["COOP_GAME_LOBBY_UPDATE"], CoopGameLobbyUpdate>
  | Envelope<ServerMessageType["PVP_GAME_LOBBY_UPDATE"], PvPGameLobbyUpdate>
  | Envelope<ServerMessageType["COOP_GAME_STATE"], CoopGameState>
  | Envelope<ServerMessageType["PVP_GAME_STATE"], PvPGameState>;

/// The in-game subset of ServerMessage, which the online session unwraps.
export type GameStateMessage = Extract<
  ServerMessage,
  { type: ServerMessageType["COOP_GAME_STATE" | "PVP_GAME_STATE"] }
>;

/// What every session's step() hands the UI: the TS stand-in for
/// shared::GameState (a std::variant, which carries its own tag). The JSON
/// payloads don't, so the session adds `mode`: LocalSession from the sim it
/// started (WebSim.step() returns the bare state), OnlineSession from the
/// envelope's `type`. Narrow on `mode`, as std::visit does on desktop.
export type GameState =
  | { mode: "coop"; state: CoopGameState }
  | { mode: "pvp"; state: PvPGameState };

export type ClientMessage =
  | Envelope<ClientMessageType["PLAYER_INPUT"], PlayerInputs>
  | Envelope<ClientMessageType["READY"], ReadyMessage>;
