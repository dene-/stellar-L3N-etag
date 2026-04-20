import { bwPalette, bwrPalette, canvas2bytes, ditheringCanvasByPalette } from '$lib/utils';
import {
	getContainSize,
	getCoverCropRect,
	pixelArtResize,
	rotateSourceToCanvas
} from '$lib/pixel-art-resize';
import type { PhotoItem } from '$lib/photo-utils';
import pica from 'pica';

const imageResizer = pica();

export type DitheringOptions = {
	mode: string;
	serpentine: boolean;
};

function createWorkingCanvas(width: number, height: number) {
	const canvas = document.createElement('canvas');
	canvas.width = width;
	canvas.height = height;
	return canvas;
}

function createCroppedCanvas(
	source: HTMLCanvasElement,
	x: number,
	y: number,
	width: number,
	height: number
) {
	const canvas = createWorkingCanvas(width, height);
	const ctx = canvas.getContext('2d', { willReadFrequently: true });
	if (!ctx) throw new Error('Canvas 2D context not available');
	ctx.drawImage(source, x, y, width, height, 0, 0, width, height);
	return canvas;
}

async function drawSourceWithResizeMode(
	source: HTMLCanvasElement,
	ctx: CanvasRenderingContext2D,
	drawX: number,
	drawY: number,
	drawWidth: number,
	drawHeight: number,
	usePixelArtResize: boolean
) {
	if (usePixelArtResize) {
		const resizedImage = pixelArtResize(source, drawWidth, drawHeight);
		ctx.putImageData(resizedImage, drawX, drawY);
		return;
	}

	const resizedCanvas = createWorkingCanvas(drawWidth, drawHeight);
	await imageResizer.resize(source, resizedCanvas);
	ctx.drawImage(resizedCanvas, drawX, drawY);
}

async function applyDitheringToCanvas(
	targetCanvas: HTMLCanvasElement,
	original: ImageData,
	dithering: DitheringOptions
) {
	const ctx = targetCanvas.getContext('2d', { willReadFrequently: true });
	if (!ctx) return;

	ctx.putImageData(original, 0, 0);

	const isBwr = dithering.mode.startsWith('bwr_');
	const kern = dithering.mode.split('_')[1] || 'Atkinson';

	await ditheringCanvasByPalette(targetCanvas, isBwr ? bwrPalette : bwPalette, kern, {
		dithSerp: dithering.serpentine
	});
}

export async function renderPhotoToCanvas(
	photo: PhotoItem,
	targetCanvas: HTMLCanvasElement,
	dithering: DitheringOptions
): Promise<ImageData | null> {
	const ctx = targetCanvas.getContext('2d', { willReadFrequently: true });
	if (!ctx) return null;

	const rotatedSource = rotateSourceToCanvas(photo.image, photo.imageRotation);

	ctx.fillStyle = '#fff';
	ctx.fillRect(0, 0, targetCanvas.width, targetCanvas.height);

	if (photo.imageFitMode === 'cover') {
		const crop = getCoverCropRect(
			rotatedSource.width,
			rotatedSource.height,
			targetCanvas.width,
			targetCanvas.height,
			photo.coverPanX,
			photo.coverPanY
		);
		const croppedSource = createCroppedCanvas(
			rotatedSource,
			crop.x,
			crop.y,
			crop.width,
			crop.height
		);
		await drawSourceWithResizeMode(
			croppedSource,
			ctx,
			0,
			0,
			targetCanvas.width,
			targetCanvas.height,
			photo.usePixelArtResize
		);
	} else if (photo.imageFitMode === 'contain') {
		const contained = getContainSize(
			rotatedSource.width,
			rotatedSource.height,
			targetCanvas.width,
			targetCanvas.height
		);
		const offsetX = Math.floor((targetCanvas.width - contained.width) / 2);
		const offsetY = Math.floor((targetCanvas.height - contained.height) / 2);
		await drawSourceWithResizeMode(
			rotatedSource,
			ctx,
			offsetX,
			offsetY,
			contained.width,
			contained.height,
			photo.usePixelArtResize
		);
	} else {
		await drawSourceWithResizeMode(
			rotatedSource,
			ctx,
			0,
			0,
			targetCanvas.width,
			targetCanvas.height,
			photo.usePixelArtResize
		);
	}

	const original = ctx.getImageData(0, 0, targetCanvas.width, targetCanvas.height);
	await applyDitheringToCanvas(targetCanvas, original, dithering);
	return original;
}

export async function renderAndBuildBuffers(
	photo: PhotoItem,
	displayWidth: number,
	displayHeight: number,
	dithering: DitheringOptions
) {
	const offscreenCanvas = createWorkingCanvas(displayWidth, displayHeight);
	await renderPhotoToCanvas(photo, offscreenCanvas, dithering);

	return {
		name: photo.name,
		black: new Uint8Array(canvas2bytes(offscreenCanvas, 'bw')),
		red: new Uint8Array(canvas2bytes(offscreenCanvas, 'bwr'))
	};
}
