import { appModule } from "../constants";
import type { GameState, PlayerIds, PlayerState } from "../messages";

const template = document.getElementById(
  "player-hud-template",
) as HTMLTemplateElement;

export class PlayerHud {
  readonly root: HTMLElement;
  private nameEl: HTMLElement;
  private pointsEl: HTMLElement;
  private livesEl: HTMLElement;
  private last = {
    name: "",
    lives: -1,
    points: -1,
    mine: false,
  };

  constructor(mountIn: HTMLElement) {
    this.root = (template.content.firstElementChild as HTMLElement).cloneNode(
      true,
    ) as HTMLElement;
    this.nameEl = this.root.querySelector(".name")!;
    this.livesEl = this.root.querySelector(".lives")!;
    this.pointsEl = this.root.querySelector(".points")!;

    mountIn.appendChild(this.root);
  }

  update(player: PlayerState, mine: boolean) {
    const name = player.name || `Player ${player.id}`;

    if (name !== this.last.name)
      this.nameEl.textContent = this.last.name = name;

    if (player.lives !== this.last.lives) {
      this.last.lives = player.lives;
      this.livesEl.textContent = String(player.lives);
      this.root.classList.toggle("dead", player.lives <= 0);
    }

    if (player.points !== this.last.points) {
      this.last.points = player.points;
      this.pointsEl.textContent = String(player.points);
    }

    if (mine !== this.last.mine) {
      this.last.mine = mine;
      this.root.classList.toggle("mine", mine);
    }
  }

  remove() {
    this.root.remove();
  }
}

export class GlobalHud {
  private bossBar = document.getElementById("boss-bar")!;
  private bossBarFill = this.bossBar.querySelector<HTMLElement>(".fill")!;
  private lastRatio = -1;

  update(gs: GameState) {
    const boss = gs.mode === "coop" ? gs.state.boss : null;
    const show =
      !!boss?.active &&
      gs.mode === "coop" &&
      gs.state.phase === appModule.CoopGamePhase.FIGHT_BOSS;

    this.bossBar.hidden = !show;
    if (!show || !boss) return;

    const ratio =
      boss.max_health > 0
        ? Math.max(0, Math.min(1, boss.health / boss.max_health))
        : 0;
    if (ratio !== this.lastRatio) {
      this.lastRatio = ratio;
      this.bossBarFill.style.width = `${ratio * 100}%`;
    }
  }
}

const left = document.getElementById("hud-left")!;
const right = document.getElementById("hud-right")!;
const players = new Map<number, PlayerHud>();
const global = new GlobalHud();

export function updateHud(gs: GameState, myIds: PlayerIds) {
  const seen = new Set<number>();

  const place = (player: PlayerState, side: HTMLElement) => {
    seen.add(player.id);
    let hud = players.get(player.id);
    if (!hud) {
      hud = new PlayerHud(side);
      players.set(player.id, hud);
    }
    hud.update(player, myIds.includes(player.id));
  };

  switch (gs.mode) {
    case "coop":
      gs.state.players.forEach((p, slot) => {
        p && place(p, slot % 2 === 0 ? left : right);
      });
      break;

    case "pvp":
      gs.state.teams.forEach((team, t) => {
        team?.players.forEach((p) => {
          p && place(p, t === 0 ? left : right);
        });
      });
      break;
  }

  for (const [id, hud] of players) {
    if (!seen.has(id)) {
      hud.remove();
      players.delete(id);
    }
  }

  global.update(gs);
}

export function clearHud() {
  for (const hud of players.values()) hud.remove();

  players.clear();
}
