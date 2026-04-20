import type { ImageFitMode, ImageRotation } from '$lib/pixel-art-resize';

export type PhotoItem = {
	id: string;
	name: string;
	image: HTMLImageElement;
	imageFitMode: ImageFitMode;
	imageRotation: ImageRotation;
	coverPanX: number;
	coverPanY: number;
	usePixelArtResize: boolean;
};

export const FLASH_IMAGE_STORAGE_BYTES = 0x37000;
const IMAGE_STORE_MAX_COUNT = 23;

export async function loadPhotoItem(file: File, index: number): Promise<PhotoItem> {
	const image = new Image();
	const objectUrl = URL.createObjectURL(file);
	image.src = objectUrl;

	await new Promise<void>((resolve, reject) => {
		image.onload = () => resolve();
		image.onerror = () => {
			URL.revokeObjectURL(objectUrl);
			reject(new Error(`Unable to load ${file.name}`));
		};
	});

	return {
		id: `${file.name}-${index}-${Date.now()}`,
		name: file.name,
		image,
		imageFitMode: 'cover',
		imageRotation: 0,
		coverPanX: 0,
		coverPanY: 0,
		usePixelArtResize: false
	};
}

export function revokePhotoUrls(items: PhotoItem[]) {
	for (const photo of items) {
		URL.revokeObjectURL(photo.image.src);
	}
}

export function clampPanOffset(value: number) {
	return Math.max(-1, Math.min(1, value));
}

export function computeMaxImageCount(displayWidth: number, displayHeight: number): number {
	const planeSize = (displayWidth * displayHeight) / 8;
	return Math.min(Math.floor(FLASH_IMAGE_STORAGE_BYTES / (planeSize * 2)), IMAGE_STORE_MAX_COUNT);
}
