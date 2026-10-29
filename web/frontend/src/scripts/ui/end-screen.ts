import { appModule } from "../constants";
import {
  type GameState,
  type PlayerIds,
  type PlayerState,
  TeamOutcome,
  type TeamState,
} from "../messages";
import { MenuNav } from "./menu-nav";

type ActionHandler = (action: "restart" | "menu") => void;
type Outcome = "won" | "lost" | "draw";

const overlay = document.getElementById("end-overlay")!;
const card = overlay.querySelector<HTMLElement>(".end-card")!;
const titleEl = card.querySelector<HTMLElement>(".end-title")!;
const subtitleEl = card.querySelector<HTMLElement>(".end-subtitle")!;
const teamsEl = card.querySelector<HTMLElement>(".end-teams")!;
const totalEl = card.querySelector<HTMLElement>(".end-total")!;
const teamTemplate = document.getElementById(
  "end-team-template",
) as HTMLTemplateElement;
const rowTemplate = document.getElementById(
  "end-row-template",
) as HTMLTemplateElement;

let onAction: ActionHandler = () => {};
const nav = new MenuNav(overlay);

overlay.addEventListener("click", (e) => {
  const action = (e.target as HTMLElement).closest<HTMLElement>("[data-action]")
    ?.dataset.action;
  if (action === "restart" || action === "menu") onAction(action);
});

export function showEndScreen(
  gs: GameState,
  myIds: PlayerIds,
  handler: ActionHandler,
) {
  onAction = handler;
  teamsEl.replaceChildren();
  totalEl.textContent = "";

  switch (gs.mode) {
    case "coop":
      showCoop(
        gs.state.phase === appModule.CoopGamePhase.WON,
        present(gs.state.players),
        myIds,
      );
      break;

    case "pvp":
      showPvP(present(gs.state.teams), myIds);
      break;
  }

  overlay.hidden = false;
  nav.activate();
}

export function hideEndScreen() {
  overlay.hidden = true;
}

function showCoop(won: boolean, players: PlayerState[], myIds: PlayerIds) {
  setHeader(
    won ? "won" : "lost",
    won ? "VICTORY" : "GAME OVER",
    won
      ? "The boss is down. Victory is yours!"
      : "The invasion won. Better luck next time!",
  );
  addTeam("", players, myIds); // coop: one group, no header

  if (players.length > 1) {
    const total = players.reduce((sum, p) => sum + p.points, 0);
    totalEl.textContent = `Team score: ${total}`;
  }
}

function showPvP(teams: TeamState[], myIds: PlayerIds) {
  const isMine = (t: TeamState) =>
    t.players.some((p) => p !== null && myIds.includes(p.id));
  const myTeams = teams.filter(isMine);
  const winner = teams.find((t) => t.outcome === TeamOutcome.WON);

  if (!winner) {
    setHeader("draw", "DRAW", "Nobody made it out.");
  } else if (myTeams.length === 1) {
    const won = myTeams[0] === winner;
    setHeader(won ? "won" : "lost", won ? "YOU WIN" : "YOU LOSE", "");
  } else {
    setHeader("won", `TEAM ${winner.id} WINS`, "");
  }

  for (const team of teams) {
    addTeam(`Team ${team.id}`, present(team.players), myIds);
  }
}

function setHeader(outcome: Outcome, title: string, subtitle: string) {
  card.classList.remove("won", "lost", "draw");
  card.classList.add(outcome);
  titleEl.textContent = title;
  subtitleEl.textContent = subtitle;
}

function addTeam(name: string, players: PlayerState[], myIds: PlayerIds) {
  const team = cloneTemplate(teamTemplate);
  team.querySelector(".end-team-name")!.textContent = name;
  const rows = team.querySelector(".end-rows")!;

  for (const p of players) {
    const row = cloneTemplate(rowTemplate);
    row.querySelector(".end-row-name")!.textContent =
      p.name || `Player ${p.id}`;
    row.querySelector(".end-row-stats")!.textContent =
      `${p.points} pts · ` +
      (p.lives > 0
        ? `${p.lives} ${p.lives === 1 ? "life" : "lives"} left`
        : "dead");
    row.classList.toggle("mine", myIds.includes(p.id));
    row.classList.toggle("dead", p.lives <= 0);
    rows.append(row);
  }

  teamsEl.append(team);
}

function cloneTemplate(t: HTMLTemplateElement): HTMLElement {
  return (t.content.firstElementChild as HTMLElement).cloneNode(
    true,
  ) as HTMLElement;
}

function present<T>(slots: (T | null)[]): T[] {
  return slots.filter((x): x is T => x !== null);
}
