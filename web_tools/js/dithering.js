const bwrPalette = [
  [0, 0, 0],
  [255, 255, 255],
  [255, 0, 0],
];

const bwPalette = [
  [0, 0, 0],
  [255, 255, 255],
];

function canvas2bytes(canvas, type = "bw") {
  const ctx = canvas.getContext("2d");
  const imageData = ctx.getImageData(0, 0, canvas.width, canvas.height);

  const arr = [];
  let buffer = [];

  for (let x = canvas.width - 1; x >= 0; x--) {
    for (let y = 0; y < canvas.height; y++) {
      const index = canvas.width * 4 * y + x * 4;
      if (type !== "bwr") {
        buffer.push(
          imageData.data[index] > 0 &&
            imageData.data[index + 1] > 0 &&
            imageData.data[index + 2] > 0
            ? 1
            : 0
        );
      } else {
        buffer.push(
          imageData.data[index] > 0 &&
            imageData.data[index + 1] === 0 &&
            imageData.data[index + 2] === 0
            ? 1
            : 0
        );
      }

      if (buffer.length === 8) {
        arr.push(parseInt(buffer.join(""), 2));
        buffer = [];
      }
    }
  }
  return arr;
}

async function bufferIntoImageData(data) {
  // Accept ImageData directly
  if (data instanceof ImageData) return data;

  // Accept canvas directly
  if (
    typeof HTMLCanvasElement !== "undefined" &&
    data instanceof HTMLCanvasElement
  ) {
    const ctx = data.getContext("2d", { willReadFrequently: true });
    return ctx.getImageData(0, 0, data.width, data.height);
  }

  const canvas = document.createElement("canvas");
  const context = canvas.getContext("2d", { willReadFrequently: true });
  if (!context) throw new Error("Canvas context is not available.");

  return new Promise((resolve, reject) => {
    const blob =
      data instanceof Blob ? data : new Blob([data], { type: "image/png" });
    const url = URL.createObjectURL(blob);
    const image = new Image();

    image.onload = () => {
      try {
        canvas.width = image.width;
        canvas.height = image.height;
        context.drawImage(image, 0, 0);
        const imgData = context.getImageData(0, 0, image.width, image.height);
        resolve(imgData);
      } finally {
        URL.revokeObjectURL(url);
      }
    };

    image.onerror = () => {
      URL.revokeObjectURL(url);
      reject(
        new Error(
          "Failed to decode image. The provided data is not a valid PNG."
        )
      );
    };

    image.src = url;
  });
}

async function ditheringCanvasByPalette(canvas, palette, type, opts = {}) {
  // ensure a valid palette is used
  palette = palette || bwrPalette;

  const ctx = canvas.getContext("2d", { willReadFrequently: true });
  const w = canvas.width;
  const h = canvas.height;

  const { dithSerp } = opts;

  const quantOptions = {
    palette,
    dithKern: type || "Atkinson",
    dithSerp,
    minHueCols: 256,
    method: 1,
    initColors: 16000,
  };

  let quant = new RgbQuant(quantOptions);

  quant.sample(canvas);
  quant.palette();

  const output = quant.reduce(canvas);

  if (
    (output instanceof Uint8Array || output instanceof Uint8ClampedArray) &&
    output.length === w * h * 4
  ) {
    const outImg = new ImageData(new Uint8ClampedArray(output), w, h);
    ctx.putImageData(outImg, 0, 0);
    return;
  }

  try {
    const imageDataConv = await bufferIntoImageData(output);
    ctx.putImageData(imageDataConv, 0, 0);
    return;
  } catch (e) {
    console.error("Quant output could not be decoded as an image:", e);
  }
}
