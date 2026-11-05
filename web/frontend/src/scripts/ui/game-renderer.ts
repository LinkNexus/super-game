import { getAsset } from "../assets";
import { appModule, canvasCtx } from "../constants";
import { setFont, toScreen } from "../helpers";
import {
  type BossState,
  type BulletState,
  type CoopGameState,
  type EnemySlot,
  EnemyType,
  type PlayerId,
  type PlayerIds,
  type PlayerState,
  type PvPGameState,
  type Vec2,
} from "../messages";
import type { GameType } from "../types";
import { drawParticles } from "./particles";

const STATUS_FONT_SIZE = 30;
const PLAYER_COLORS = {
  mainPlayer: "#ffffff",
  players: [
    "#BF40BF",
    "#F4A261",
    "#E9C46A",
    "#2A9D8F",
    "#457B9D",
    "#7B2CBF",
    "#F284B6",
    "#264653",
  ],
  playerEnemy: "#EE4B2B",
};
// Never hand a friendly ship the enemy colour.
const FRIENDLY_COLORS = PLAYER_COLORS.players.filter(
  (c) => c.toLowerCase() !== PLAYER_COLORS.playerEnemy.toLowerCase(),
);
const PLAYER_MARKER_GAP = 10;
const PLAYER_MARKER_FONT_SIZE = 15;
const BULLET_COLOR = "yellow";

export type TextStyle = {
  size?: number;
  color?: string;
  align?: CanvasTextAlign;
  baseline?: CanvasTextBaseline;
};

function drawText(
  text: string,
  x: number,
  y: number,
  {
    size = STATUS_FONT_SIZE,
    color = "white",
    align = "left",
    baseline = "top",
  }: TextStyle = {},
) {
  setFont(size);
  canvasCtx.fillStyle = color;
  canvasCtx.textAlign = align;
  canvasCtx.textBaseline = baseline;
  canvasCtx.fillText(text, x, y);
}

const offCanvasesMap = new Map<string, HTMLCanvasElement>();

function drawImage(
  image: HTMLImageElement,
  x: number,
  y: number,
  width: number,
  height: number,
  angle: number = 0,
  tintColor?: string,
) {
  canvasCtx.save();

  canvasCtx.translate(x + width / 2, y + height / 2);
  canvasCtx.rotate(angle);

  let source: CanvasImageSource = image;

  if (tintColor) {
    const key = `${image.src}_${width}_${height}_${tintColor}`;
    let off = offCanvasesMap.get(key);

    if (!off) {
      off = document.createElement("canvas");
      off.width = width;
      off.height = height;
      const offCtx = off.getContext("2d")!;
      offCtx.drawImage(image, 0, 0, width, height);
      offCtx.globalCompositeOperation = "multiply";
      offCtx.fillStyle = tintColor;
      offCtx.fillRect(0, 0, width, height);
      offCtx.globalCompositeOperation = "destination-in";
      offCtx.drawImage(image, 0, 0, width, height);
      offCanvasesMap.set(key, off);
    }

    source = off;
  }

  canvasCtx.drawImage(source, -width / 2, -height / 2, width, height);
  canvasCtx.restore();
}

export function clearScreen() {
  canvasCtx.fillStyle = "black";
  canvasCtx.clearRect(0, 0, appModule.SCREEN_WIDTH, appModule.SCREEN_HEIGHT);
}

export function drawCoopGame(state: CoopGameState, myIds: PlayerIds) {
  for (let i = 0; i < state.players.length; ++i) {
    const player = state.players[i];
    if (!player || player.lives < 1) continue;
    drawPlayer(
      player,
      myIds.indexOf(player.id),
      playerColor(player.id, i, myIds, false),
    );
  }

  drawBullets(state.bullets);
  drawEnemies(state.enemies, state.enemies_offset);
  drawParticles();
  drawBoss(state.boss);
}

export function drawPvPGame(
  state: PvPGameState,
  myIds: PlayerIds,
  gameType: GameType,
) {
  const myTeams = new Set(
    state.teams.flatMap((t, idx) =>
      t?.players.some((p) => p && myIds.includes(p.id)) ? [idx] : [],
    ),
  );
  // One team of mine online: that's "my team". None or both: no opponents,
  // every ship its own colour, like local.
  const myTeamIdx =
    gameType === "online" && myTeams.size === 1 ? [...myTeams][0]! : -1;

  state.teams.forEach((team, teamIdx) => {
    team?.players.forEach((p, rank) => {
      if (!p) return;

      const color = playerColor(
        p.id,
        teamIdx * appModule.MAX_PLAYERS_PER_TEAM + rank,
        myIds,
        myTeamIdx !== -1 && teamIdx !== myTeamIdx,
      );
      if (p.lives < 1) return; // after the colour, so deaths shift nothing

      drawPlayer(p, myIds.indexOf(p.id), color);
    });
  });

  drawBullets(state.bullets);
  drawParticles();
}

/// Depends on ids and roster slots only, never on who's alive, so a ship
/// keeps its colour all match. `slot`: coop, index in `state.players`; PvP,
/// teamIdx * MAX_PLAYERS_PER_TEAM + rank.
function playerColor(
  id: PlayerId,
  slot: number,
  myIds: PlayerIds,
  isOpponent: boolean,
): string {
  if (isOpponent) return PLAYER_COLORS.playerEnemy;

  const mine = myIds.indexOf(id);
  if (mine === 0) return PLAYER_COLORS.mainPlayer;

  // This machine's players take the first colours, everyone else follows by
  // slot, so the two groups never collide.
  const idx = mine > 0 ? mine - 1 : appModule.MAX_PLAYERS_PER_CLIENT - 1 + slot;
  return FRIENDLY_COLORS[idx % FRIENDLY_COLORS.length]!;
}

function drawPlayer(player: PlayerState, playerIdx: number, color: string) {
  const playerShipImage = getAsset("playerShip");
  const size = appModule.SIM_PLAYER_SIZE;
  const scale = (size * 2) / playerShipImage.naturalWidth;

  const screenPos = toScreen(player.position);
  drawImage(
    playerShipImage,
    screenPos.x - size,
    screenPos.y - size,
    playerShipImage.naturalWidth * scale,
    playerShipImage.naturalHeight * scale,
    player.orientation,
    color,
  );

  if (playerIdx >= 0) {
    const drawnHalfHeight =
      (playerShipImage.naturalHeight *
        ((size * 2) / playerShipImage.naturalWidth)) /
      2;

    const d = drawnHalfHeight + PLAYER_MARKER_GAP + PLAYER_MARKER_FONT_SIZE;

    const markerPos = toScreen({
      x: player.position.x + d * Math.sin(player.orientation),
      y: player.position.y - d * Math.cos(player.orientation),
    });

    drawText(`P${playerIdx + 1}`, markerPos.x, markerPos.y, {
      align: "center",
      size: PLAYER_MARKER_FONT_SIZE,
    });
  }
}

function drawBullets(bullets: BulletState[]) {
  for (const bullet of bullets) {
    if (!bullet.active) continue;

    const screenPos = toScreen(bullet.position);

    canvasCtx.fillStyle = BULLET_COLOR;
    canvasCtx.fillRect(
      screenPos.x - appModule.SIM_BULLET_WIDTH / 2,
      screenPos.y - appModule.SIM_BULLET_HEIGHT / 2,
      appModule.SIM_BULLET_WIDTH,
      appModule.SIM_BULLET_HEIGHT,
    );
  }
}

function drawEnemies(enemies: EnemySlot[], offset: Vec2) {
  for (let i = 0; i < enemies.length; ++i) {
    if (!enemies[i]![0]) continue;

    const image = getAsset(
      enemies[i]![1] === EnemyType.TYPE_1 ? "enemy1" : "enemy2",
    );
    const posX =
      offset.x +
      (i % appModule.SIM_ENEMY_COLS) *
        (appModule.SIM_ENEMY_WIDTH + appModule.SIM_ENEMIES_POOL_SPACING_X);
    const poxY =
      offset.y -
      Math.floor(i / appModule.SIM_ENEMY_COLS) *
        (appModule.SIM_ENEMY_HEIGHT + appModule.SIM_ENEMIES_POOL_SPACING_Y);
    const scale = appModule.SIM_ENEMY_WIDTH / image.naturalWidth;
    const screenPos = toScreen({
      x: posX,
      y: poxY,
    });

    drawImage(
      image,
      screenPos.x - (image.naturalWidth * scale) / 2,
      screenPos.y - (image.naturalHeight * scale) / 2,
      image.naturalWidth * scale,
      image.naturalHeight * scale,
    );
  }
}

function drawBoss(boss: BossState) {
  if (!boss.active) return;

  const screenPos = toScreen(boss.position);

  drawImage(
    getAsset("boss"),
    screenPos.x - appModule.SIM_BOSS_WIDTH / 2,
    screenPos.y - appModule.SIM_BOSS_HEIGHT / 2,
    appModule.SIM_BOSS_WIDTH,
    appModule.SIM_BOSS_HEIGHT,
  );
}
