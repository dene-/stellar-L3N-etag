type LabColor = {
	l: number;
	a: number;
	b: number;
};

export type ImageFitMode = 'stretch' | 'contain' | 'cover';
export type ImageRotation = 0 | 90 | 180 | 270;

function clampByte(value: number): number {
	return Math.max(0, Math.min(255, Math.round(value)));
}

function median(values: number[]): number {
	const sorted = [...values].sort((left, right) => left - right);
	const middle = Math.floor(sorted.length / 2);

	if (sorted.length % 2 === 1) {
		return sorted[middle];
	}

	return (sorted[middle - 1] + sorted[middle]) / 2;
}

function srgbToLinear(channel: number): number {
	const normalized = channel / 255;
	return normalized <= 0.04045 ? normalized / 12.92 : ((normalized + 0.055) / 1.055) ** 2.4;
}

function linearToSrgb(channel: number): number {
	return channel <= 0.0031308 ? channel * 12.92 : 1.055 * channel ** (1 / 2.4) - 0.055;
}

function xyzPivot(value: number): number {
	return value > 0.008856 ? Math.cbrt(value) : 7.787 * value + 16 / 116;
}

function labPivot(value: number): number {
	const cubed = value ** 3;
	return cubed > 0.008856 ? cubed : (value - 16 / 116) / 7.787;
}

function rgbToLab(red: number, green: number, blue: number): LabColor {
	const r = srgbToLinear(red);
	const g = srgbToLinear(green);
	const b = srgbToLinear(blue);

	const x = (r * 0.4124564 + g * 0.3575761 + b * 0.1804375) / 0.95047;
	const y = r * 0.2126729 + g * 0.7151522 + b * 0.072175;
	const z = (r * 0.0193339 + g * 0.119192 + b * 0.9503041) / 1.08883;

	const fx = xyzPivot(x);
	const fy = xyzPivot(y);
	const fz = xyzPivot(z);

	return {
		l: 116 * fy - 16,
		a: 500 * (fx - fy),
		b: 200 * (fy - fz)
	};
}

function labToRgb(lightness: number, a: number, b: number): [number, number, number] {
	const fy = (lightness + 16) / 116;
	const fx = fy + a / 500;
	const fz = fy - b / 200;

	const x = 0.95047 * labPivot(fx);
	const y = labPivot(fy);
	const z = 1.08883 * labPivot(fz);

	const linearRed = x * 3.2404542 + y * -1.5371385 + z * -0.4985314;
	const linearGreen = x * -0.969266 + y * 1.8760108 + z * 0.041556;
	const linearBlue = x * 0.0556434 + y * -0.2040259 + z * 1.0572252;

	return [
		clampByte(linearToSrgb(Math.max(0, linearRed)) * 255),
		clampByte(linearToSrgb(Math.max(0, linearGreen)) * 255),
		clampByte(linearToSrgb(Math.max(0, linearBlue)) * 255)
	];
}

function getSourceDimensions(source: CanvasImageSource): { width: number; height: number } {
	if (source instanceof HTMLImageElement) {
		return {
			width: source.naturalWidth || source.width,
			height: source.naturalHeight || source.height
		};
	}

	if (source instanceof HTMLCanvasElement) {
		return { width: source.width, height: source.height };
	}

	if (typeof ImageBitmap !== 'undefined' && source instanceof ImageBitmap) {
		return { width: source.width, height: source.height };
	}

	if (typeof OffscreenCanvas !== 'undefined' && source instanceof OffscreenCanvas) {
		return { width: source.width, height: source.height };
	}

	if (source instanceof SVGImageElement) {
		return {
			width: source.width.baseVal.value,
			height: source.height.baseVal.value
		};
	}

	if (source instanceof HTMLVideoElement) {
		return {
			width: source.videoWidth || source.width,
			height: source.videoHeight || source.height
		};
	}

	throw new Error('Unsupported image source for pixel-art resize');
}

function extractImageData(source: CanvasImageSource): ImageData {
	const { width, height } = getSourceDimensions(source);
	const canvas = document.createElement('canvas');
	canvas.width = width;
	canvas.height = height;

	const context = canvas.getContext('2d', { willReadFrequently: true });
	if (!context) {
		throw new Error('Canvas 2D context not available');
	}

	context.drawImage(source, 0, 0, width, height);
	return context.getImageData(0, 0, width, height);
}

function createCanvas(width: number, height: number): HTMLCanvasElement {
	const canvas = document.createElement('canvas');
	canvas.width = width;
	canvas.height = height;
	return canvas;
}

export function rotateSourceToCanvas(
	source: CanvasImageSource,
	rotation: ImageRotation
): HTMLCanvasElement {
	const { width, height } = getSourceDimensions(source);
	const isQuarterTurn = rotation === 90 || rotation === 270;
	const canvas = createCanvas(isQuarterTurn ? height : width, isQuarterTurn ? width : height);
	const context = canvas.getContext('2d', { willReadFrequently: true });

	if (!context) {
		throw new Error('Canvas 2D context not available');
	}

	context.save();

	switch (rotation) {
		case 90:
			context.translate(canvas.width, 0);
			context.rotate(Math.PI / 2);
			break;
		case 180:
			context.translate(canvas.width, canvas.height);
			context.rotate(Math.PI);
			break;
		case 270:
			context.translate(0, canvas.height);
			context.rotate(-Math.PI / 2);
			break;
		default:
			break;
	}

	context.drawImage(source, 0, 0, width, height);
	context.restore();
	return canvas;
}

export function getContainSize(
	sourceWidth: number,
	sourceHeight: number,
	targetWidth: number,
	targetHeight: number
): { width: number; height: number } {
	const scale = Math.min(targetWidth / sourceWidth, targetHeight / sourceHeight);
	return {
		width: Math.max(1, Math.round(sourceWidth * scale)),
		height: Math.max(1, Math.round(sourceHeight * scale))
	};
}

export function getCoverCropRect(
	sourceWidth: number,
	sourceHeight: number,
	targetWidth: number,
	targetHeight: number,
	offsetXRatio = 0,
	offsetYRatio = 0
): { x: number; y: number; width: number; height: number } {
	const sourceAspect = sourceWidth / sourceHeight;
	const targetAspect = targetWidth / targetHeight;
	const clampedOffsetXRatio = Math.max(-1, Math.min(1, offsetXRatio));
	const clampedOffsetYRatio = Math.max(-1, Math.min(1, offsetYRatio));

	if (sourceAspect > targetAspect) {
		const width = Math.max(1, Math.round(sourceHeight * targetAspect));
		const availableX = Math.max(0, sourceWidth - width);
		return {
			x: Math.round(((clampedOffsetXRatio + 1) / 2) * availableX),
			y: 0,
			width,
			height: sourceHeight
		};
	}

	const height = Math.max(1, Math.round(sourceWidth / targetAspect));
	const availableY = Math.max(0, sourceHeight - height);
	return {
		x: 0,
		y: Math.round(((clampedOffsetYRatio + 1) / 2) * availableY),
		width: sourceWidth,
		height
	};
}

function selectContrastAwareLightness(values: number[], centerLightness: number): number {
	const medianLightness = median(values);
	const meanLightness = values.reduce((sum, value) => sum + value, 0) / values.length;
	const maxLightness = Math.max(...values);
	const minLightness = Math.min(...values);

	if (
		medianLightness < meanLightness &&
		maxLightness - medianLightness > medianLightness - minLightness
	) {
		return minLightness;
	}

	if (
		medianLightness > meanLightness &&
		maxLightness - medianLightness < medianLightness - minLightness
	) {
		return maxLightness;
	}

	return centerLightness;
}

export function pixelArtResize(
	source: CanvasImageSource,
	targetWidth: number,
	targetHeight: number
): ImageData {
	const sourceImageData = extractImageData(source);
	const output = new ImageData(targetWidth, targetHeight);
	const sourceWidth = sourceImageData.width;
	const sourceHeight = sourceImageData.height;
	const sourcePixels = sourceImageData.data;
	const outputPixels = output.data;

	for (let targetY = 0; targetY < targetHeight; targetY += 1) {
		const startY = Math.floor((targetY * sourceHeight) / targetHeight);
		const endY = Math.max(startY + 1, Math.ceil(((targetY + 1) * sourceHeight) / targetHeight));

		for (let targetX = 0; targetX < targetWidth; targetX += 1) {
			const startX = Math.floor((targetX * sourceWidth) / targetWidth);
			const endX = Math.max(startX + 1, Math.ceil(((targetX + 1) * sourceWidth) / targetWidth));
			const centerX = startX + Math.floor((endX - startX) / 2);
			const centerY = startY + Math.floor((endY - startY) / 2);

			const lightnessValues: number[] = [];
			const aValues: number[] = [];
			const bValues: number[] = [];
			let centerLightness = 0;

			for (let sourceY = startY; sourceY < endY; sourceY += 1) {
				for (let sourceX = startX; sourceX < endX; sourceX += 1) {
					const index = (sourceY * sourceWidth + sourceX) * 4;
					const color = rgbToLab(
						sourcePixels[index],
						sourcePixels[index + 1],
						sourcePixels[index + 2]
					);

					lightnessValues.push(color.l);
					aValues.push(color.a);
					bValues.push(color.b);

					if (sourceX === centerX && sourceY === centerY) {
						centerLightness = color.l;
					}
				}
			}

			const lightness = selectContrastAwareLightness(lightnessValues, centerLightness);
			const a = median(aValues);
			const b = median(bValues);
			const [red, green, blue] = labToRgb(lightness, a, b);
			const outputIndex = (targetY * targetWidth + targetX) * 4;

			outputPixels[outputIndex] = red;
			outputPixels[outputIndex + 1] = green;
			outputPixels[outputIndex + 2] = blue;
			outputPixels[outputIndex + 3] = 255;
		}
	}

	return output;
}
