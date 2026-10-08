// Shared utilities for image processing and hex conversions
// Note: RgbQuant is JS-only; types are loose here
// eslint-disable-next-line @typescript-eslint/ban-ts-comment
// @ts-ignore
import RgbQuant from 'rgbquant';

export const bwrPalette: number[][] = [
	[0, 0, 0],
	[255, 255, 255],
	[255, 0, 0]
];

export const bwPalette: number[][] = [
	[0, 0, 0],
	[255, 255, 255]
];

export function hexToBytes(hex: string): Uint8Array {
	const clean = hex.replace(/\s|0x|,/gi, '');
	if (clean.length % 2 !== 0 || !/^[0-9a-f]*$/i.test(clean)) {
		throw new Error('Invalid hex string');
	}
	const bytes = new Uint8Array(clean.length / 2);
	for (let c = 0; c < clean.length; c += 2) bytes[c / 2] = parseInt(clean.slice(c, c + 2), 16);
	return bytes;
}

export function bytesToHex(data: ArrayBuffer | ArrayLike<number> | ArrayBufferLike): string {
	return new Uint8Array(data as ArrayBuffer).reduce(
		(memo, i) => memo + ('0' + i.toString(16)).slice(-2),
		''
	);
}

export function intToHex(intIn: number, bytes = 4): string {
	return intIn.toString(16).padStart(bytes * 2, '0');
}

export function decimalToHex(d: number, padding: number = 2): string {
	const hex = Number(d).toString(16);

	if (hex.startsWith('-')) {
		return '-' + hex.slice(1).padStart(padding, '0');
	}
	return hex.padStart(padding, '0');
}

// Packs the canvas into a panel plane: columns from the rightmost to the leftmost, each column
// ceil(height / 8) bytes with the top pixel in the MSB (see Firmware/src/domain/epd_canvas.h). Rows
// past the canvas height pad each column to whole bytes: white in the black plane, none in red.
export function canvas2bytes(canvas: HTMLCanvasElement, type: 'bw' | 'bwr' = 'bw'): Uint8Array {
	const ctx = canvas.getContext('2d', { willReadFrequently: true });
	if (!ctx) return new Uint8Array(0);
	const { width, height } = canvas;
	const data = ctx.getImageData(0, 0, width, height).data;
	const columnBytes = Math.ceil(height / 8);
	const arr = new Uint8Array(width * columnBytes);
	const padBit = type === 'bwr' ? 0 : 1;
	let byteIndex = 0;

	for (let x = width - 1; x >= 0; x--) {
		for (let row = 0; row < columnBytes * 8; row += 8) {
			let byte = 0;
			for (let y = row; y < row + 8; y++) {
				let bit = padBit;
				if (y < height) {
					const index = (width * y + x) * 4;
					bit =
						type === 'bwr'
							? Number(data[index] > 0 && data[index + 1] === 0 && data[index + 2] === 0)
							: Number(data[index] > 0 && data[index + 1] > 0 && data[index + 2] > 0);
				}
				byte = (byte << 1) | bit;
			}
			arr[byteIndex++] = byte;
		}
	}
	return arr;
}

export async function bufferIntoImageData(
	data: ImageData | HTMLCanvasElement | Blob | ArrayBuffer | Uint8Array
): Promise<ImageData> {
	// Accept ImageData directly
	if (typeof ImageData !== 'undefined' && data instanceof ImageData) return data;

	// Accept canvas directly
	if (typeof HTMLCanvasElement !== 'undefined' && data instanceof HTMLCanvasElement) {
		const ctx = data.getContext('2d', { willReadFrequently: true });
		if (!ctx) throw new Error('Canvas 2D context not available');
		return ctx.getImageData(0, 0, data.width, data.height);
	}

	const canvas = document.createElement('canvas');
	const context = canvas.getContext('2d', { willReadFrequently: true });
	if (!context) throw new Error('Canvas context is not available.');

	return new Promise((resolve, reject) => {
		const blob = data instanceof Blob ? data : new Blob([data as BlobPart], { type: 'image/png' });
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
			reject(new Error('Failed to decode image. The provided data is not a valid PNG.'));
		};

		image.src = url;
	});
}

export async function ditheringCanvasByPalette(
	canvas: HTMLCanvasElement,
	palette: number[][],
	kern: string,
	opts: { dithSerp?: boolean } = {}
): Promise<void> {
	const ctx = canvas.getContext('2d', { willReadFrequently: true });
	if (!ctx) return;
	const w = canvas.width;
	const h = canvas.height;

	const { dithSerp } = opts;
	const quantOptions = {
		palette,
		dithKern: kern || 'Atkinson',
		dithSerp,
		minHueCols: 256,
		method: 1,
		initColors: 16000
	} as Record<string, unknown>;

	const quant = new RgbQuant(quantOptions);
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
		const imageDataConv = await bufferIntoImageData(output as ImageData);
		ctx.putImageData(imageDataConv, 0, 0);
	} catch (e) {
		console.error('Quant output could not be decoded as an image:', e);
	}
}
