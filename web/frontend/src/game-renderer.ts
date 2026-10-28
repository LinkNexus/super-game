import { getAsset, type ImageName } from "./assets";
import { appModule, canvasCtx } from "./constants";
import { countOptional, setFont, toScreen } from "./helpers";
import type {
  BossState,
  BulletState,
  CoopGameState,
  EnemySlot,
  PlayerIds,
  PlayerState,
  PvPGameState,
} from "./messages";
import { drawParticles } from "./particles";
import { LocalSession } from "./session";
import type { GameScreen, GameType } from "./types";

const PADDING = 10;
const MARGIN_BETWEEN_STATUS_TEXTS = 5;
const STATUS_FONT_SIZE = 25;
const PLAYER_LIVE_SIZE = 40;
const PLAYER_LIVE_PADDING = 10;
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
const BOSS_HEALTH_BAR_WIDTH = 300;
const BOSS_HEALTH_BAR_Y_OFFSET = 10;
const BOSS_HEALTH_BAR_HEIGHT = 25;
const BOSS_HEALTH_BAR_COLOR = "red";
const BOSS_HEALTH_BAR_BG_COLOR = "gray";
const BOSS_HEALTH_BAR_BORDER = "white";

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

const drawStatusTextBesideImage = (
  imageName: ImageName,
  text: string,
  yOffset: number,
) => {
  const image = getAsset(imageName);
  const imageScale = PLAYER_LIVE_SIZE / image.naturalWidth;
  const imageX = appModule.SCREEN_WIDTH - PLAYER_LIVE_SIZE - PADDING;

  const centerY = yOffset + PLAYER_LIVE_SIZE / 2;
  drawText(text, imageX, centerY, {
    align: "right",
    baseline: "middle",
  });

  drawImage(
    image,
    imageX + PLAYER_LIVE_PADDING,
    centerY - (image.naturalHeight * imageScale) / 2,
    image.naturalWidth * imageScale,
    image.naturalHeight * imageScale,
  );

  return yOffset + PLAYER_LIVE_SIZE + MARGIN_BETWEEN_STATUS_TEXTS;
};

export function draw(screen: GameScreen) {
  canvasCtx.fillStyle = "black";
  canvasCtx.clearRect(0, 0, appModule.SCREEN_WIDTH, appModule.SCREEN_HEIGHT);

  switch (screen.kind) {
    case "playing":
    case "playMenu":
      drawGame(screen);
      break;

    case "menu":
    case "end":
      break;
  }
}

function drawGame({
  kind,
  match: { state, session },
}: GameScreen & { kind: "playing" | "playMenu" }) {
  if (!state) return;

  const gameType: GameType =
    session instanceof LocalSession ? "local" : "online";
  const myIds = session.getPlayerIds();

  switch (state.mode) {
    case "coop":
      drawCoopGame(state.state, myIds, gameType);
      break;

    case "pvp":
      drawPvPGame(state.state, myIds, gameType);
      break;
  }

  if (kind === "playMenu") {
    canvasCtx.fillStyle = "rgba(0, 0, 0, 0.5)";
    canvasCtx.fillRect(0, 0, appModule.SCREEN_WIDTH, appModule.SCREEN_HEIGHT);

    canvasCtx.fillStyle = "white";
    setFont(50);

    canvasCtx.textAlign = "center";
    canvasCtx.textBaseline = "middle";
    canvasCtx.fillText(
      "GAME PAUSED",
      appModule.SCREEN_WIDTH / 2,
      appModule.SCREEN_HEIGHT / 2,
    );
  }
}

function drawCoopGame(
  state: CoopGameState,
  myIds: PlayerIds,
  gameType: GameType,
) {
  let yOffset = 50;

  for (let i = 0; i < state.players.length; ++i) {
    const player = state.players[i];

    if (!player) continue;

    const playerIdx = myIds.indexOf(player.id);

    yOffset = drawPlayerStats(
      player,
      countOptional(state.players),
      yOffset,
      gameType,
      playerIdx >= 0,
    );

    drawPlayer(player, playerIdx);
  }

  drawBullets(state.bullets);
  drawEnemies(state.enemies, state.enemies_offset_x, state.enemies_offset_y);
  drawParticles();
  drawBoss(state.boss, state.phase === appModule.CoopGamePhase.FIGHT_BOSS);
}

function drawPvPGame(
  state: PvPGameState,
  myIds: PlayerIds,
  gameType: GameType,
) {}

function drawPlayerStats(
  player: PlayerState,
  playerCount: number,
  yOffset: number,
  gameType: GameType,
  belongsToMe: boolean,
): number {
  if (playerCount > 1) {
    drawText(
      (player.name.length > 0 ? player.name : `Player ${player.id}`) +
        (gameType === "online" && belongsToMe ? " (You)" : ""),
      appModule.SCREEN_WIDTH - PADDING,
      yOffset,
      {
        align: "right",
      },
    );

    yOffset += STATUS_FONT_SIZE + MARGIN_BETWEEN_STATUS_TEXTS;
  }

  yOffset =
    drawStatusTextBesideImage(
      player.lives > 0 ? "heart" : "skull",
      player.lives > 0 ? `Health: ${player.lives.toString()}` : "",
      yOffset,
    ) + MARGIN_BETWEEN_STATUS_TEXTS;

  yOffset =
    drawStatusTextBesideImage(
      "star",
      `Points: ${player.points.toString()}`,
      yOffset,
    ) + MARGIN_BETWEEN_STATUS_TEXTS;

  return yOffset;
}

function drawPlayer(player: PlayerState, playerIdx: number) {
  const playerShipImage = getAsset("playerShip");
  const color =
    playerIdx === 0
      ? PLAYER_COLORS.mainPlayer
      : PLAYER_COLORS.players[playerIdx % PLAYER_COLORS.players.length];
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

function drawBoss(boss: BossState, canDrawHealthBar: boolean) {
  if (!boss.active) return;

  if (canDrawHealthBar) {
    const barX = (appModule.SCREEN_WIDTH - BOSS_HEALTH_BAR_WIDTH) / 2;
    const healthRatio =
      boss.max_health > 0
        ? Math.max(0, Math.min(1, boss.health / boss.max_health))
        : 0;

    canvasCtx.lineWidth = 2;
    canvasCtx.fillStyle = BOSS_HEALTH_BAR_BG_COLOR;
    canvasCtx.fillRect(
      barX,
      BOSS_HEALTH_BAR_Y_OFFSET,
      BOSS_HEALTH_BAR_WIDTH,
      BOSS_HEALTH_BAR_HEIGHT,
    );
    canvasCtx.fillStyle = BOSS_HEALTH_BAR_COLOR;
    canvasCtx.fillRect(
      barX,
      BOSS_HEALTH_BAR_Y_OFFSET,
      BOSS_HEALTH_BAR_WIDTH * healthRatio,
      BOSS_HEALTH_BAR_HEIGHT,
    );
    canvasCtx.strokeStyle = BOSS_HEALTH_BAR_BORDER;
    canvasCtx.strokeRect(
      barX,
      BOSS_HEALTH_BAR_Y_OFFSET,
      BOSS_HEALTH_BAR_WIDTH,
      BOSS_HEALTH_BAR_HEIGHT,
    );
  }

  const screenPos = toScreen(boss.position);

  drawImage(
    getAsset("boss"),
    screenPos.x - appModule.SIM_BOSS_WIDTH / 2,
    screenPos.y - appModule.SIM_BOSS_HEIGHT / 2,
    appModule.SIM_BOSS_WIDTH,
    appModule.SIM_BOSS_HEIGHT,
  );
}
