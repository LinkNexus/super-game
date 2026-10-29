const FILENAMES = [
  "boss",
  "playerShip",
  "heart",
  "enemy1",
  "enemy2",
  "skull",
  "star",
] as const;
export type ImageName = (typeof FILENAMES)[number];

const images = {} as Record<ImageName, HTMLImageElement>;

export async function loadAssets() {
  const promises = await Promise.all(
    FILENAMES.map(async (filename) => {
      const img = new Image();
      img.src = `${filename}.png`;
      await img.decode();
      return img;
    }),
  );

  FILENAMES.forEach((filename, i) => {
    images[filename] = promises[i]!;
  });
}

export function getAsset(name: ImageName) {
  return images[name];
}
