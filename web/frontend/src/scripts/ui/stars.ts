type Star = {
  x: number;
  y: number;
  speed: number;
  size: number;
  alpha: number;
};

let width = 0,
  height = 0;
const reduceMotion = window.matchMedia(
  "(prefers-reduced-motion: reduce)",
).matches;
const bg = document.getElementById("stars") as HTMLCanvasElement;
const bgCtx = bg.getContext("2d")!;
let stars: Star[] = [];

function resize() {
  const dpr = window.devicePixelRatio || 1;
  width = bg.clientWidth;
  height = bg.clientHeight;
  bg.width = width * dpr;
  bg.height = height * dpr;
  bgCtx.setTransform(dpr, 0, 0, dpr, 0, 0);

  const count = Math.round((width * height) / 10000);
  stars = Array.from(
    {
      length: count,
    },
    () => randomStar(Math.random() * height),
  );
}

function randomStar(y: number): Star {
  return {
    x: Math.random() * innerWidth,
    y,
    speed: 20 + Math.random() * 80,
    size: Math.random() * 2 + 0.5,
    alpha: 0.3 + Math.random() * 0.7,
  };
}

export function updateStars(dt: number) {
  for (const star of stars) {
    star.y += star.speed * dt * (reduceMotion ? 0.1 : 1);
    if (star.y > innerHeight) {
      Object.assign(star, randomStar(-star.size));
    }
  }
}

export function drawStars() {
  bgCtx.clearRect(0, 0, innerWidth, innerHeight);
  for (const star of stars) {
    bgCtx.fillStyle = `rgba(255, 255, 255, ${star.alpha})`;
    bgCtx.fillRect(star.x, star.y, star.size, star.size);
  }
}

new ResizeObserver(resize).observe(bg);
