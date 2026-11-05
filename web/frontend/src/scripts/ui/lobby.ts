import type {
  LobbyUpdate,
  PlayerId,
  PlayerIds,
  PlayerInfo,
  TeamInfo,
} from "../messages";
import { MenuNav } from "./menu-nav";

/// What the lobby's buttons ask for. The lobby never changes itself: the
/// next LobbyUpdate from the server is what shows the result.
export type LobbyActions = {
  onReady: (playerId: PlayerId, isReady: boolean) => void;
  onSwitchTeam: (playerId: PlayerId, teamId: number) => void;
  onLeave: () => void;
};

/// Coop is one group without a team; PvP one group per team.
type Group = { teamId: number | null; seats: (PlayerInfo | null)[] };

const overlay = document.getElementById("lobby-overlay")!;
const card = overlay.querySelector<HTMLElement>(".lobby-card")!;
const titleEl = card.querySelector<HTMLElement>(".lobby-title")!;
const subtitleEl = card.querySelector<HTMLElement>(".lobby-subtitle")!;
const groupsEl = card.querySelector<HTMLElement>(".lobby-groups")!;
const statusEl = card.querySelector<HTMLElement>(".lobby-status")!;
const groupTemplate = document.getElementById(
  "lobby-group-template",
) as HTMLTemplateElement;
const seatTemplate = document.getElementById(
  "lobby-seat-template",
) as HTMLTemplateElement;

let actions: LobbyActions = {
  onReady: () => {},
  onSwitchTeam: () => {},
  onLeave: () => {},
};
const nav = new MenuNav(overlay);

overlay.addEventListener("click", (e) => {
  const b = (e.target as HTMLElement).closest<HTMLElement>("[data-action]");
  if (!b) return;

  switch (b.dataset.action) {
    case "ready":
      actions.onReady(Number(b.dataset.player), b.dataset.ready === "true");
      break;

    case "switch":
      actions.onSwitchTeam(Number(b.dataset.player), Number(b.dataset.team));
      break;

    case "leave":
      actions.onLeave();
      break;
  }
});

/// Shows the lobby, or redraws it on every LobbyUpdate. Focus stays on the
/// same button across redraws, so another player readying doesn't move it.
export function showLobby(
  lobby: LobbyUpdate,
  myIds: PlayerIds,
  handlers: LobbyActions,
) {
  actions = handlers;
  const focusedKey = overlay.hidden
    ? undefined
    : (document.activeElement as HTMLElement | null)?.dataset?.key;

  const groups = toGroups(lobby);
  const isMine = (p: PlayerInfo) => myIds.includes(p.id);

  setHeader(lobby, groups);
  groupsEl.replaceChildren(
    ...groups.map((g) => groupEl(g, groups, lobby, isMine)),
  );
  statusEl.textContent = status(lobby, groups, isMine);

  overlay.hidden = false;
  nav.activate(handlers.onLeave);
  restoreFocus(focusedKey);
}

export function hideLobby() {
  overlay.hidden = true;
}

function toGroups(lobby: LobbyUpdate): Group[] {
  switch (lobby.mode) {
    case "coop":
      return [{ teamId: null, seats: lobby.update.players }];

    case "pvp":
      return lobby.update.teams
        .filter((t): t is TeamInfo => t !== null)
        .map((t) => ({
          teamId: t.id,
          seats: t.players.slice(0, lobby.update.team_size),
        }));
  }
}

function setHeader(lobby: LobbyUpdate, groups: Group[]) {
  switch (lobby.mode) {
    case "coop":
      titleEl.textContent = "Co-op lobby";
      subtitleEl.textContent = "Fighting the invasion";
      break;

    case "pvp":
      titleEl.textContent = "PvP lobby";
      subtitleEl.textContent = groups
        .map(() => lobby.update.team_size)
        .join(" vs ");
      break;
  }
}

function groupEl(
  group: Group,
  groups: Group[],
  lobby: LobbyUpdate,
  isMine: (p: PlayerInfo) => boolean,
): HTMLElement {
  const el = cloneTemplate(groupTemplate);
  el.querySelector(".lobby-group-name")!.textContent =
    group.teamId === null ? "" : `Team ${group.teamId}`;

  const seats = el.querySelector(".lobby-seats")!;
  for (const p of group.seats) {
    seats.append(seatEl(p, group, groups, lobby, isMine));
  }

  return el;
}

function seatEl(
  p: PlayerInfo | null,
  group: Group,
  groups: Group[],
  lobby: LobbyUpdate,
  isMine: (p: PlayerInfo) => boolean,
): HTMLElement {
  const el = cloneTemplate(seatTemplate);
  const nameEl = el.querySelector<HTMLElement>(".lobby-seat-name")!;
  const stateEl = el.querySelector<HTMLElement>(".lobby-seat-state")!;
  const actionsEl = el.querySelector<HTMLElement>(".lobby-seat-actions")!;

  if (!p) {
    el.classList.add("open");
    nameEl.textContent = "Open seat";
    return el;
  }

  const mine = isMine(p);
  el.classList.toggle("mine", mine);
  el.classList.toggle("ready", p.is_ready);
  nameEl.textContent = p.name || `Player ${p.id}`;
  stateEl.textContent = p.is_ready ? "Ready" : "Not ready";

  if (!mine) return el;

  actionsEl.append(
    button(p.is_ready ? "Unready" : "Ready", `ready:${p.id}`, {
      action: "ready",
      player: String(p.id),
      ready: String(!p.is_ready),
    }),
  );

  if (lobby.mode === "pvp") {
    for (const other of groups) {
      if (other === group || other.teamId === null) continue;

      const b = button(
        `To team ${other.teamId}`,
        `switch:${p.id}:${other.teamId}`,
        { action: "switch", player: String(p.id), team: String(other.teamId) },
      );
      b.disabled = other.seats.every((s) => s !== null); // full
      actionsEl.append(b);
    }
  }

  return el;
}

function status(
  lobby: LobbyUpdate,
  groups: Group[],
  isMine: (p: PlayerInfo) => boolean,
): string {
  const players = groups.flatMap((g) =>
    g.seats.filter((s): s is PlayerInfo => s !== null),
  );
  const ready = players.filter((p) => p.is_ready).length;
  const counts = `${ready}/${players.length} ready`;

  if (lobby.update.game_started) return "Starting...";

  switch (lobby.mode) {
    // Coop needs a second machine, not just a second player.
    case "coop":
      return players.every(isMine)
        ? "Waiting for another machine to join..."
        : `${counts} · starts when everyone is ready`;

    case "pvp": {
      const open = groups.reduce(
        (n, g) => n + g.seats.filter((s) => s === null).length,
        0,
      );
      return open > 0
        ? `${counts} · waiting for ${open} more player${open > 1 ? "s" : ""}`
        : `${counts} · starts when everyone is ready`;
    }
  }
}

function button(
  label: string,
  key: string,
  data: Record<string, string>,
): HTMLButtonElement {
  const b = document.createElement("button");
  b.type = "button";
  b.className = "btn";
  b.textContent = label;
  b.dataset.key = key;
  Object.assign(b.dataset, data);
  return b;
}

/// Back to the button focused before the redraw; if it's gone (e.g. the
/// switch button after a switch), that player's Ready button.
function restoreFocus(key: string | undefined) {
  if (!key) return;

  const find = (k: string) =>
    overlay.querySelector<HTMLElement>(`[data-key="${k}"]:not([disabled])`);
  const player = key.split(":")[1];
  const el = find(key) ?? (player ? find(`ready:${player}`) : null);
  if (el) nav.focus(el);
}

function cloneTemplate(t: HTMLTemplateElement): HTMLElement {
  return (t.content.firstElementChild as HTMLElement).cloneNode(
    true,
  ) as HTMLElement;
}
