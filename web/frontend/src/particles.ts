import { canvasCtx } from "./constants";
import type { EnemyType } from "./messages";

type Particle = {
  x: number;
  y: number;
  dx: number;
  dy: number;
  lifetime: number;
  maxLifetime: number;
  color: string;
  size: number;
};

const MAX_PARTICLES = 128;
const particles: Particle[] = Array.from({ length: MAX_PARTICLES }, () => ({
  x: 0,
  y: 0,
  dx: 0,
  dy: 0,
  lifetime: 0,
  maxLifetime: 0,
  color: "white",
  size: 3,
}));
const GRAVITY = 20;
const DAMPING_FACTOR = 0.98;

export function updateParticles(dt: number) {
  for (const particle of particles) {
    if (particle.lifetime <= 0) continue;

    particle.x += DAMPING_FACTOR * particle.dx * dt;
    particle.y += (DAMPING_FACTOR * particle.dy + GRAVITY) * dt;

    particle.lifetime -= dt;
    if (particle.lifetime < 0) particle.lifetime = 0;
  }
}

export function spawnEnemyExplosion(
  position: { x: number; y: number },
  enemyType: EnemyType,
) {
  const numParticles = 8 + Math.floor(Math.random() * 8);

  for (let i = 0; i < numParticles; i++) {
    for (const particle of particles) {
      if (particle.lifetime > 0) continue;

      const angle = Math.random() * 2 * Math.PI;
      const speed = Math.random() * 200;

      particle.x = position.x;
      particle.y = position.y;
      particle.dx = Math.cos(angle) * speed;
      particle.dy = Math.sin(angle) * speed;
      particle.lifetime = 0.35 + (Math.random() * 50) / 100;
      particle.maxLifetime = particle.lifetime;
      particle.size = 2 + Math.random() * 6;

      switch (enemyType) {
        case 0:
          particle.color = "255, 165, 0";
          break;
        case 1:
          particle.color = "128, 0, 128";
          break;
      }

      break;
    }
  }
}

export function drawParticles() {
  for (const particle of particles) {
    if (particle.lifetime <= 0) continue;

    let ratio = particle.lifetime / particle.maxLifetime;
    if (ratio < 0) ratio = 0;
    if (ratio > 1) ratio = 1;

    const color = `rgba(${particle.color}, ${ratio})`;

    if (particle.size <= 3) {
      canvasCtx.beginPath();
      canvasCtx.arc(particle.x, particle.y, particle.size, 0, 2 * Math.PI);
      canvasCtx.fillStyle = color;
      canvasCtx.fill();
    } else {
      canvasCtx.fillStyle = color;
      canvasCtx.fillRect(
        particle.x - particle.size / 2,
        particle.y - particle.size / 2,
        particle.size,
        particle.size,
      );
    }
  }
}
