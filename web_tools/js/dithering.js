const bwrPalette = [
  [0, 0, 0, 255],
  [255, 255, 255, 255],
  [255, 0, 0, 255]
]

const bwPalette = [
  [0, 0, 0, 255],
  [255, 255, 255, 255],
]

function dithering(ctx, width, height, threshold, type) {
  const bayerThresholdMap = [
    [  15, 135,  45, 165 ],
    [ 195,  75, 225, 105 ],
    [  60, 180,  30, 150 ],
    [ 240, 120, 210,  90 ]
  ];

  const lumR = [];
  const lumG = [];
  const lumB = [];
  for (let i=0; i<256; i++) {
    lumR[i] = i*0.299;
    lumG[i] = i*0.587;
    lumB[i] = i*0.114;
  }
  const imageData = ctx.getImageData(0, 0, width, height);

  const imageDataLength = imageData.data.length;

  // Greyscale luminance (sets r pixels to luminance of rgb)
  for (let i = 0; i <= imageDataLength; i += 4) {
    imageData.data[i] = Math.floor(lumR[imageData.data[i]] + lumG[imageData.data[i+1]] + lumB[imageData.data[i+2]]);
  }

  const w = imageData.width;
  let newPixel, err;

  for (let currentPixel = 0; currentPixel <= imageDataLength; currentPixel+=4) {

    if (type ==="none") {
      // No dithering
      imageData.data[currentPixel] = imageData.data[currentPixel] < threshold ? 0 : 255;
    } else if (type ==="bayer") {
      // 4x4 Bayer ordered dithering algorithm
      var x = currentPixel/4 % w;
      var y = Math.floor(currentPixel/4 / w);
      var map = Math.floor( (imageData.data[currentPixel] + bayerThresholdMap[x%4][y%4]) / 2 );
      imageData.data[currentPixel] = (map < threshold) ? 0 : 255;
    } else if (type ==="floydsteinberg") {
      // Floyda€"Steinberg dithering algorithm
      newPixel = imageData.data[currentPixel] < 129 ? 0 : 255;
      err = Math.floor((imageData.data[currentPixel] - newPixel) / 16);
      imageData.data[currentPixel] = newPixel;

      imageData.data[currentPixel       + 4 ] += err*7;
      imageData.data[currentPixel + 4*w - 4 ] += err*3;
      imageData.data[currentPixel + 4*w     ] += err*5;
      imageData.data[currentPixel + 4*w + 4 ] += err*1;
    } else {
      // Bill Atkinson's dithering algorithm
      newPixel = imageData.data[currentPixel] < threshold ? 0 : 255;
      err = Math.floor((imageData.data[currentPixel] - newPixel) / 8);
      imageData.data[currentPixel] = newPixel;

      imageData.data[currentPixel       + 4 ] += err;
      imageData.data[currentPixel       + 8 ] += err;
      imageData.data[currentPixel + 4*w - 4 ] += err;
      imageData.data[currentPixel + 4*w     ] += err;
      imageData.data[currentPixel + 4*w + 4 ] += err;
      imageData.data[currentPixel + 8*w     ] += err;
    }

    // Set g and b pixels equal to r
    imageData.data[currentPixel + 1] = imageData.data[currentPixel + 2] = imageData.data[currentPixel];
  }

  ctx.putImageData(imageData, 0, 0);
}

function canvas2bytes(canvas, type='bw') {
  const ctx = canvas.getContext("2d");
  const imageData = ctx.getImageData(0, 0, canvas.width, canvas.height);

  const arr = [];
  let buffer = [];

  for (let x = canvas.width - 1; x >= 0; x--) {
    for (let y = 0; y < canvas.height; y++) {
      const index = (canvas.width * 4 * y) + x * 4;
      if (type !== 'bwr') {
        buffer.push(imageData.data[index] > 0 && imageData.data[index+1] > 0 && imageData.data[index+2] > 0 ? 1 : 0);
      } else {
        buffer.push(imageData.data[index] > 0 && imageData.data[index+1] === 0 && imageData.data[index+2] === 0 ? 1 : 0);
      }

      if (buffer.length === 8) {
        arr.push(parseInt(buffer.join(''), 2));
        buffer = [];
      }
    }
  }
  return arr;
}

function getColorDistance(rgba1, rgba2) {
  // NOTE: fix channel order bug (was r,b,g). Keep alpha ignored.
  const [r1, g1, b1] = rgba1;
  const [r2, g2, b2] = rgba2;

  const rm = (r1 + r2 ) / 2;

  const r = r1 - r2;
  const g = g1 - g2;
  const b = b1 - b2;

  return Math.sqrt((2 + rm / 256) * r * r + 4 * g * g + (2 + (255 - rm) / 256) * b * b);
}

function getNearColor(pixel, palette) {
  let minDistance = 255 * 255 * 3 + 1;
  let paletteIndex = 0;

  for (let i = 0; i < palette.length; i++) {
    const targetColor = palette[i];
    const distance = getColorDistance(pixel, targetColor);
    if (distance < minDistance) {
      minDistance = distance;
      paletteIndex = i;
    }
  }

  return palette[paletteIndex];
}


function getNearColorV2(color, palette) {
  let minDistanceSquared = 255*255 + 255*255 + 255*255 + 1;

  let bestIndex = 0;
  for (let i = 0; i < palette.length; i++) {
      let rdiff = (color[0] & 0xff) - (palette[i][0] & 0xff);
      let gdiff = (color[1] & 0xff) - (palette[i][1] & 0xff);
      let bdiff = (color[2] & 0xff) - (palette[i][2] & 0xff);
      let distanceSquared = rdiff*rdiff + gdiff*gdiff + bdiff*bdiff;
      if (distanceSquared < minDistanceSquared) {
          minDistanceSquared = distanceSquared;
          bestIndex = i;
      }
  }
  return palette[bestIndex];

}

function updatePixel(imageData, index, color) {
  imageData[index] = color[0];
  imageData[index+1] = color[1];
  imageData[index+2] = color[2];
  imageData[index+3] = color[3];
}

function getColorErr(color1, color2, rate) {
  const res = [];
  for (let i = 0; i < 3; i++) {
    res.push(Math.floor((color1[i] - color2[i]) / rate));
  }
  return res;
}

function updatePixelErr(imageData, index, err, rate) {
  // Apply error with clamping to [0,255] to avoid channel wrap.
  const r = imageData[index] + err[0] * rate;
  const g = imageData[index+1] + err[1] * rate;
  const b = imageData[index+2] + err[2] * rate;
  imageData[index]   = r < 0 ? 0 : (r > 255 ? 255 : r);
  imageData[index+1] = g < 0 ? 0 : (g > 255 ? 255 : g);
  imageData[index+2] = b < 0 ? 0 : (b > 255 ? 255 : b);
}

// Lightweight perceptual helpers
function makeGammaLUT(gamma) {
  const lut = new Array(256);
  const g = gamma || 2.2;
  for (let i = 0; i < 256; i++) {
    lut[i] = Math.pow(i / 255, g);
  }
  return lut;
}

function colorDistanceLinear(c1, c2, lut) {
  // Euclidean distance in linearized sRGB (ignore alpha)
  const r1 = lut[c1[0] & 0xff];
  const g1 = lut[c1[1] & 0xff];
  const b1 = lut[c1[2] & 0xff];
  const r2 = lut[c2[0] & 0xff];
  const g2 = lut[c2[1] & 0xff];
  const b2 = lut[c2[2] & 0xff];
  const dr = r1 - r2, dg = g1 - g2, db = b1 - b2;
  return dr*dr + dg*dg + db*db;
}

function isStrongRed(c) {
  // Heuristic: prefer mapping to red only if it's clearly red.
  const r = c[0], g = c[1], b = c[2];
  const maxGB = g > b ? g : b;
  return (r - maxGB) >= 50 && r >= 160 && maxGB <= 110;
}

function getNearColorPerceptual(color, palette, lut, opts) {
  let bestIdx = 0;
  let bestD = 1e9;
  const biasRedAway = opts && opts.biasRedAway ? opts.biasRedAway : 0; // add to distance for red if not strong
  for (let i = 0; i < palette.length; i++) {
    let d = colorDistanceLinear(color, palette[i], lut);
    // Optional red biasing for readability: avoid muddy reds for grayscale content
    if (palette[i][0] === 255 && palette[i][1] === 0 && palette[i][2] === 0) {
      if (!isStrongRed(color)) d += biasRedAway;
    }
    if (d < bestD) {
      bestD = d;
      bestIdx = i;
    }
  }
  return palette[bestIdx];
}

function ditheringCanvasByPalette(canvas, palette, type, opts = {}) {
  palette = palette || bwrPalette;

  const ctx = canvas.getContext('2d');
  const imageData = ctx.getImageData(0, 0, canvas.width, canvas.height);
  const w = imageData.width;
  const h = imageData.height;

  const serpentine = opts.serpentine !== false; // default true
  const gamma = opts.gamma || 2.2;
  const redBias = opts.redBias !== undefined ? opts.redBias : 0.03; // distance penalty in linear space
  const lut = makeGammaLUT(gamma);
  const jitter = opts.jitter || 0; // 0..8, adds noise (in 8-bit units) to decorrelate patterns
  const orderedStrength = opts.orderedStrength || 10; // 0..32, magnitude of ordered mask influence (in 8-bit units)

  const isFS = type === "bwr_floydsteinberg";
  // Only treat explicit Atkinson type as Atkinson to avoid swallowing other modes
  const isAtkinson = (type === "bwr_Atkinson" || type === "bwr_atkinson");
  const isOrderedBayer8 = type === "bwr_bayer8";
  const isOrderedBlue8  = type === "bwr_blue8";

  // Helper to add error safely within bounds
  function addErr(x, y, dx, dy, err, rate) {
    const nx = x + dx;
    const ny = y + dy;
    if (nx < 0 || nx >= w || ny < 0 || ny >= h) return;
    const nidx = (ny * w + nx) * 4;
    updatePixelErr(imageData.data, nidx, err, rate);
  }

  const redPenalty = redBias; // used as additive penalty in linear distance

  // 8x8 Bayer threshold map (0..63)
  const bayer8 = [
    [ 0, 48, 12, 60,  3, 51, 15, 63],
    [32, 16, 44, 28, 35, 19, 47, 31],
    [ 8, 56,  4, 52, 11, 59,  7, 55],
    [40, 24, 36, 20, 43, 27, 39, 23],
    [ 2, 50, 14, 62,  1, 49, 13, 61],
    [34, 18, 46, 30, 33, 17, 45, 29],
    [10, 58,  6, 54,  9, 57,  5, 53],
    [42, 26, 38, 22, 41, 25, 37, 21]
  ];

  // Small 8x8 blue-noise mask (0..63), handcrafted; spreads power to higher frequencies
  const blue8 = [
    [30, 48, 12, 60,  2, 50, 18, 34],
    [44, 22, 40, 26, 58, 10, 38, 14],
    [ 8, 56,  4, 52, 16, 32,  6, 54],
    [36, 20, 46, 28, 42, 24, 62,  0],
    [ 1, 49, 13, 61,  3, 51, 19, 35],
    [33, 17, 45, 29, 57, 11, 39, 15],
    [ 9, 55,  5, 53, 23, 41,  7, 59],
    [37, 21, 47, 31, 43, 25, 63, 27]
  ];

  function orderedMask01(x, y) {
    const m = isOrderedBlue8 ? blue8 : bayer8;
    return m[y & 7][x & 7] / 63; // 0..1
  }

  function applyPerturb(color, delta) {
    // delta is in 8-bit units; add equally to RGB for neutral lightness modulation
    const r = Math.max(0, Math.min(255, color[0] + delta));
    const g = Math.max(0, Math.min(255, color[1] + delta));
    const b = Math.max(0, Math.min(255, color[2] + delta));
    return [r, g, b, color[3]];
  }

  for (let y = 0; y < h; y++) {
    const dir = serpentine && (y % 2 === 1) ? -1 : 1;
    const xStart = dir === 1 ? 0 : w - 1;
    const xEnd = dir === 1 ? w : -1;
    for (let x = xStart; x !== xEnd; x += dir) {
      const idx = (y * w + x) * 4;
      let curr = imageData.data.slice(idx, idx + 4);

      // Optional jitter to reduce banding/quantization contours
      if (jitter > 0 && (isFS || isAtkinson)) {
        const j = (Math.random() * 2 - 1) * jitter; // [-jitter, +jitter]
        curr = applyPerturb(curr, j);
      }

      // Get nearest palette color using gamma-linearized distance and optional red bias
      let newColor;

      if (isOrderedBayer8 || isOrderedBlue8) {
        // Ordered palette dithering: modulate decision with spatial mask
        const t = orderedMask01(x, y) - 0.5;           // [-0.5, 0.5]
        const delta = t * 2 * orderedStrength;        // scale to 8-bit units
        const perturbed = applyPerturb(curr, delta);
        newColor = getNearColorPerceptual(perturbed, palette, lut, { biasRedAway: redPenalty });
        updatePixel(imageData.data, idx, newColor);
        continue; // no diffusion for ordered modes
      } else {
        newColor = getNearColorPerceptual(curr, palette, lut, {
          biasRedAway: redPenalty
        });
      }

      const errRateDiv = isFS ? 16 : 8;
      const err = getColorErr(curr, newColor, errRateDiv);
      updatePixel(imageData.data, idx, newColor);

      if (isFS) {
        // Floyd–Steinberg with serpentine
        if (dir === 1) {
          addErr(x, y, +1,  0, err, 7);   // right
          addErr(x, y, -1, +1, err, 3);   // below-left
          addErr(x, y,  0, +1, err, 5);   // below
          addErr(x, y, +1, +1, err, 1);   // below-right
        } else {
          addErr(x, y, -1,  0, err, 7);   // left (since scanning right->left)
          addErr(x, y, +1, +1, err, 3);   // below-right (mirrored)
          addErr(x, y,  0, +1, err, 5);   // below
          addErr(x, y, -1, +1, err, 1);   // below-left (mirrored)
        }
      } else if (isAtkinson) {
        // Atkinson diffusion (serpentine + bounds-safe)
        if (dir === 1) {
          addErr(x, y, +1,  0, err, 1);   // right
          addErr(x, y, +2,  0, err, 1);   // right+1
          addErr(x, y, -1, +1, err, 1);   // below-left
          addErr(x, y,  0, +1, err, 1);   // below
          addErr(x, y, +1, +1, err, 1);   // below-right
          addErr(x, y,  0, +2, err, 1);   // two rows down
        } else {
          addErr(x, y, -1,  0, err, 1);   // left (mirrored)
          addErr(x, y, -2,  0, err, 1);   // left-1
          addErr(x, y, +1, +1, err, 1);   // below-right (mirrored)
          addErr(x, y,  0, +1, err, 1);   // below
          addErr(x, y, -1, +1, err, 1);   // below-left (mirrored)
          addErr(x, y,  0, +2, err, 1);   // two rows down
        }
      } else {
        // No diffusion
      }
    }
  }
  ctx.putImageData(imageData, 0, 0);
}