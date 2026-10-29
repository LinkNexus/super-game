import { getAsset } from "../assets";
import { appModule, canvasCtx } from "../constants";
import { setFont, toScreen } from "../helpers";
import type {
  BossState,
  BulletState,
  CoopGameState,
  EnemySlot,
  PlayerIds,
  PlayerState,
  PvPGameState,
} from "../messages";
import type { GameType } from "../types";
import { drawParticles } from "./particles";

const STATUS_FONT_SIZE = 30;
const PLAYER_COLORS = {
  mainPlayer: "#ffffff",
  players: [
    "#E63946",
    "#F4A261",
    "#E9C46A",
    "#2A9D8F",
    "#457B9D",
    "#7B2CBF",
    "#F284B6",
    "#264653",
  ],
  playerEnemy: "#E63946",
};
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

  if (tintColor) {
    const offscreenCanvas = document.createElement("canvas");
    offscreenCanvas.width = width;
    offscreenCanvas.height = height;

    const offscreenCtx = offscreenCanvas.getContext("2d")!;

    offscreenCtx.drawImage(image, 0, 0, width, height);
    offscreenCtx.globalCompositeOperation = "source-atop";
    offscreenCtx.fillStyle = tintColor;
    offscreenCtx.fillRect(0, 0, width, height);
  }

  canvasCtx.drawImage(image, -width / 2, -height / 2, width, height);

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
    const playerIdx = myIds.indexOf(player.id);
    drawPlayer(
      player,
      playerIdx,
      playerIdx === 0
        ? PLAYER_COLORS.mainPlayer
        : PLAYER_COLORS.players[playerIdx % PLAYER_COLORS.players.length]!,
    );
  }

  drawBullets(state.bullets);
  drawEnemies(state.enemies, state.enemies_offset_x, state.enemies_offset_y);
  drawParticles();
  drawBoss(state.boss);
}

export function drawPvPGame(
  state: PvPGameState,
  myIds: PlayerIds,
  gameType: GameType,
) {
  const isOnThisMachine = (player: PlayerState) => {
    return player.id !== 0 && myIds.includes(player.id);
  };
  const thisMachineTeamIdx = state.teams.findIndex((t) => {
    return t?.players.some((p) => p && isOnThisMachine(p));
  });

  let colorIdx = 0;
  for (let teamIdx = 0; teamIdx < state.teams.length; ++teamIdx) {
    const team = state.teams[teamIdx];
    if (!team) continue;

    team.players.forEach((p) => {
      if (!p || p.lives < 1) return;

      let color = PLAYER_COLORS.playerEnemy;
      const playerIdxOnThisMachine = myIds.indexOf(p.id);

      if (gameType === "local" || thisMachineTeamIdx === teamIdx) {
        color =
          playerIdxOnThisMachine === 0
            ? PLAYER_COLORS.mainPlayer
            : PLAYER_COLORS.players[colorIdx++ % PLAYER_COLORS.players.length]!;
      }

      drawPlayer(p, playerIdxOnThisMachine, color);
    });
  }

  drawBullets(state.bullets);
  drawParticles();
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

    const markerPos = toScreen(
      new appModule.Vec2D(
        player.position.x,
        player.position.y - drawnHalfHeight - PLAYER_MARKER_GAP,
      ).rotated(player.orientation),
    );

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

function drawEnemies(enemies: EnemySlot[], offsetX: number, offsetY: number) {
  for (let i = 0; i < enemies.length; ++i) {
    if (enemies[i]![0] === 0) continue;

    const image = getAsset(enemies[i]![1] === 0 ? "enemy1" : "enemy2");
    const posX =
      offsetX +
      (i % appModule.SIM_ENEMIES_POOL_COLS) *
        (appModule.SIM_ENEMY_WIDTH + appModule.SIM_ENEMIES_POOL_SPACING_X);
    const poxY =
      offsetY -
      Math.floor(i / appModule.SIM_ENEMIES_POOL_COLS) *
        (appModule.SIM_ENEMY_HEIGHT + appModule.SIM_ENEMIES_POOL_SPACING_Y);
    const scale = appModule.SIM_ENEMY_WIDTH / image.naturalWidth;
    const screenPos = toScreen(new appModule.Vec2D(posX, poxY));

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
