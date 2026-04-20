<script lang="ts">
	import { bleConnectionStore, DISPLAY_MODEL_OPTIONS } from '../stores/connectionStore.svelte';
	import { logStore } from '../stores/logStore.svelte';
	import {
		bwPalette,
		bwrPalette,
		canvas2bytes,
		ditheringCanvasByPalette,
		bytesToHex
	} from '$lib/utils';
	import {
		getContainSize,
		getCoverCropRect,
		pixelArtResize,
		rotateSourceToCanvas,
		type ImageFitMode,
		type ImageRotation
	} from '$lib/pixel-art-resize';
	import pica from 'pica';

	// Helpers to build hex payloads
	const hb = (n: number) => n.toString(16).padStart(2, '0');
	const imageResizer = pica();
	const FLASH_IMAGE_STORAGE_BYTES = 0x37000;

	type PhotoItem = {
		id: string;
		name: string;
		image: HTMLImageElement;
		imageFitMode: ImageFitMode;
		imageRotation: ImageRotation;
		coverPanX: number;
		coverPanY: number;
		usePixelArtResize: boolean;
	};

	let charByteHex = $state('ff');

	let ditheringMode: string = $state('bwr_Atkinson');
	let serpentine = $state(false);
	let photoItems = $state<PhotoItem[]>([]);
	let selectedPhotoIndex = $state(0);
	let slideshowIntervalSeconds = $state(60);
	let canvasEl: HTMLCanvasElement | null = null;

	let firmwareArray = $state('');

	async function handlePageError(error: unknown) {
		console.error(error);
		logStore.addLog('Error: ' + (error instanceof Error ? error.message : String(error)));
	}

	function getSelectedPhoto() {
		return photoItems[selectedPhotoIndex] ?? null;
	}

	function imageControlsDisabled() {
		return (
			!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware || !getSelectedPhoto()
		);
	}

	function revokePhotoUrls(items: PhotoItem[]) {
		for (const photo of items) {
			URL.revokeObjectURL(photo.image.src);
		}
	}

	function onCanvasReady(node: HTMLCanvasElement) {
		canvasEl = node;
		const ctx = node.getContext('2d', { willReadFrequently: true });

		if (!ctx) return;

		ctx.fillStyle = '#fff';
		ctx.fillRect(0, 0, node.width, node.height);

		return {
			destroy() {
				canvasEl = null;
			}
		};
	}

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
		if (!ctx) {
			throw new Error('Canvas 2D context not available');
		}

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

	async function applyDitheringToCanvas(targetCanvas: HTMLCanvasElement, original: ImageData) {
		const ctx = targetCanvas.getContext('2d', { willReadFrequently: true });
		if (!ctx) return;

		ctx.putImageData(original, 0, 0);

		const isBwr = ditheringMode.startsWith('bwr_');
		const kern = ditheringMode.split('_')[1] || 'Atkinson';

		await ditheringCanvasByPalette(targetCanvas, isBwr ? bwrPalette : bwPalette, kern, {
			dithSerp: serpentine
		});
	}

	async function renderPhotoToCanvas(photo: PhotoItem, targetCanvas: HTMLCanvasElement) {
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
		await applyDitheringToCanvas(targetCanvas, original);
		return original;
	}

	async function renderSelectedPhoto() {
		if (!canvasEl) return;

		const selectedPhoto = getSelectedPhoto();
		const ctx = canvasEl.getContext('2d', { willReadFrequently: true });
		if (!ctx) return;

		if (!selectedPhoto) {
			ctx.fillStyle = '#fff';
			ctx.fillRect(0, 0, canvasEl.width, canvasEl.height);
			return;
		}

		await renderPhotoToCanvas(selectedPhoto, canvasEl);
	}

	async function resizeCanvasToDisplay(targetWidth: number, targetHeight: number) {
		if (!canvasEl) return;

		if (canvasEl.width === targetWidth && canvasEl.height === targetHeight) return;
		canvasEl.width = targetWidth;
		canvasEl.height = targetHeight;
		await renderSelectedPhoto();
	}

	$effect(() => {
		void resizeCanvasToDisplay(bleConnectionStore.displayWidth, bleConnectionStore.displayHeight);
	});

	async function loadPhotoItem(file: File, index: number): Promise<PhotoItem> {
		const image = new Image();
		const objectUrl = URL.createObjectURL(file);

		image.src = objectUrl;

		await new Promise<void>((resolve, reject) => {
			image.onload = () => resolve();
			image.onerror = () => reject(new Error(`Unable to load ${file.name}`));
		});

		return {
			id: `${file.name}-${index}-${Date.now()}`,
			name: file.name,
			image,
			imageFitMode: 'contain',
			imageRotation: 0,
			coverPanX: 0,
			coverPanY: 0,
			usePixelArtResize: false
		};
	}

	async function handleImageFile(event: Event) {
		const input = event.target as HTMLInputElement;

		if (!canvasEl || !input.files || input.files.length === 0) return;

		try {
			const nextItems = await Promise.all(
				Array.from(input.files).map((file, index) => loadPhotoItem(file, index))
			);

			revokePhotoUrls(photoItems);
			photoItems = nextItems;
			selectedPhotoIndex = 0;

			if (nextItems.length > 1 && slideshowIntervalSeconds < 1) {
				slideshowIntervalSeconds = 60;
			}

			await renderSelectedPhoto();
		} catch (error) {
			await handlePageError(error);
		}
	}

	function handleFirmwareFile(event: Event) {
		const input = event.target as HTMLInputElement;

		var reader = new FileReader();

		if (!input.files || !input.files[0]) {
			logStore.addLog('No file selected');
			return;
		}

		reader.readAsArrayBuffer(input.files[0]);
		let fileName = input.files[0].name;

		reader.onload = function () {
			firmwareArray = bytesToHex(this.result as ArrayBuffer);
			if (firmwareArray.substring(16, 24) != '4b4e4c54') {
				alert('Select file is no telink firmware .bin');
				logStore.addLog('Select file is no telink firmware .bin');

				firmwareArray = '';
				return;
			}

			logStore.addLog(`[${fileName}] selected, size: ${firmwareArray.length / 2} bytes`);
		};
	}

	function handleDisplayModelChange(event: Event) {
		const select = event.currentTarget;
		if (!(select instanceof HTMLSelectElement)) return;

		bleConnectionStore.setDisplayModel(Number(select.value));
	}

	function resetFileInputValue(event: Event) {
		const input = event.currentTarget;
		if (!(input instanceof HTMLInputElement)) return;

		input.value = '';
	}

	async function applyDithering() {
		await renderSelectedPhoto();
	}

	async function rerenderImageWithCurrentResizeMode() {
		await renderSelectedPhoto();
	}

	function handleImageFitModeChange(event: Event) {
		const select = event.currentTarget;
		if (!(select instanceof HTMLSelectElement)) return;

		const selectedPhoto = getSelectedPhoto();
		if (!selectedPhoto) return;

		selectedPhoto.imageFitMode = select.value as ImageFitMode;
		void rerenderImageWithCurrentResizeMode();
	}

	function clampPanOffset(value: number) {
		return Math.max(-1, Math.min(1, value));
	}

	function nudgeCoverCrop(deltaX: number, deltaY: number) {
		const selectedPhoto = getSelectedPhoto();
		if (!selectedPhoto) return;

		selectedPhoto.coverPanX = clampPanOffset(selectedPhoto.coverPanX + deltaX);
		selectedPhoto.coverPanY = clampPanOffset(selectedPhoto.coverPanY + deltaY);
		void rerenderImageWithCurrentResizeMode();
	}

	function resetCoverCropPosition() {
		const selectedPhoto = getSelectedPhoto();
		if (!selectedPhoto) return;

		selectedPhoto.coverPanX = 0;
		selectedPhoto.coverPanY = 0;
		void rerenderImageWithCurrentResizeMode();
	}

	function rotateImage(delta: 90 | -90) {
		const selectedPhoto = getSelectedPhoto();
		if (!selectedPhoto) return;

		const nextRotation = (selectedPhoto.imageRotation + delta + 360) % 360;
		selectedPhoto.imageRotation = nextRotation as ImageRotation;
		void rerenderImageWithCurrentResizeMode();
	}

	function togglePixelArtResize(event: Event) {
		const input = event.currentTarget;
		const selectedPhoto = getSelectedPhoto();

		if (!(input instanceof HTMLInputElement) || !selectedPhoto) return;

		selectedPhoto.usePixelArtResize = input.checked;
		void rerenderImageWithCurrentResizeMode();
	}

	function switchSelectedPhoto(delta: number) {
		if (!photoItems.length) return;

		selectedPhotoIndex = (selectedPhotoIndex + delta + photoItems.length) % photoItems.length;
		void renderSelectedPhoto();
	}

	async function buildRenderedImageBuffers(photo: PhotoItem) {
		const offscreenCanvas = createWorkingCanvas(
			bleConnectionStore.displayWidth,
			bleConnectionStore.displayHeight
		);
		await renderPhotoToCanvas(photo, offscreenCanvas);

		return {
			name: photo.name,
			black: new Uint8Array(canvas2bytes(offscreenCanvas, 'bw')),
			red: new Uint8Array(canvas2bytes(offscreenCanvas, 'bwr'))
		};
	}

	async function uploadImage() {
		if (!photoItems.length) return;

		const planeSize = (bleConnectionStore.displayWidth * bleConnectionStore.displayHeight) / 8;
		const maxImageCount = Math.floor(FLASH_IMAGE_STORAGE_BYTES / (planeSize * 2));

		if (photoItems.length > maxImageCount) {
			alert(`Too many photos for device flash. Maximum for this display is ${maxImageCount}.`);
			return;
		}

		const renderedPhotos = [];
		for (const photo of photoItems) {
			renderedPhotos.push(await buildRenderedImageBuffers(photo));
		}

		await bleConnectionStore.uploadImageSet(
			renderedPhotos,
			photoItems.length > 1 ? slideshowIntervalSeconds : 0
		);
	}

	function setTimeNow() {
		const now = new Date();
		const unix = Math.floor(now.getTime() / 1000);
		const y = now.getFullYear();
		const m = now.getMonth() + 1;
		const d = now.getDate();
		const wd = now.getDay(); // 0=Sun
		const hex =
			'dd' +
			hb((unix >>> 24) & 0xff) +
			hb((unix >>> 16) & 0xff) +
			hb((unix >>> 8) & 0xff) +
			hb(unix & 0xff) +
			hb((y >>> 8) & 0xff) +
			hb(y & 0xff) +
			hb(m) +
			hb(d) +
			hb(wd);
		bleConnectionStore.sendRxTxCommand(hex);
	}
</script>

<div class="flex flex-col flex-grow lg:flex-row gap-3 pb-5">
	<div class="flex-grow flex flex-col gap-3">
		<div class="bg-base-300 rounded-lg p-3 prose max-w-none">
			<h3>Connection</h3>
			<div class="flex gap-3">
				<button
					class="btn btn-primary"
					onclick={() => bleConnectionStore.preConnect()}
					disabled={bleConnectionStore.isFlashingFirmware || bleConnectionStore.connected}
				>
					Connect
				</button>
				<button
					class="btn btn-secondary"
					onclick={() => bleConnectionStore.disconnect()}
					disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
				>
					Disconnect
				</button>
			</div>
			<div class="mt-3 text-sm opacity-80">
				<div>Device: {bleConnectionStore.connectedDeviceName || 'Not connected'}</div>
				<div>
					Display: {bleConnectionStore.deviceModelName} ({bleConnectionStore.displayWidth}x{bleConnectionStore.displayHeight})
					via {bleConnectionStore.displaySource}
				</div>
			</div>
		</div>

		<div class="collapse collapse-arrow bg-base-300 rounded-lg">
			<input type="radio" name="accordion" checked={true} />
			<div class="collapse-title text-lg font-semibold">Temperature & time</div>
			<div class="collapse-content">
				<div class="prose max-w-none p-0">
					<div class="flex gap-3 flex-wrap">
						<button
							class="btn btn-accent"
							onclick={setTimeNow}
							disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
						>
							Set time now
						</button>
						<button
							class="btn btn-primary"
							onclick={() => bleConnectionStore.sendRxTxCommand('e2aa')}
							disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
						>
							Get temperature
						</button>
					</div>
				</div>
			</div>
		</div>

		<div class="collapse collapse-arrow bg-base-300 rounded-lg">
			<input type="radio" name="accordion" />
			<div class="collapse-title text-lg font-semibold">EPD control</div>
			<div class="collapse-content">
				<div class="prose max-w-none p-0 flex items-start flex-col gap-3">
					<div class="flex flex-wrap gap-3">
						<h4 class="w-full">Display model</h4>
						<select
							class="select select-bordered select-primary w-60"
							value={String(bleConnectionStore.deviceModel)}
							onchange={handleDisplayModelChange}
							disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
						>
							{#each DISPLAY_MODEL_OPTIONS as option (option.model)}
								<option value={option.model}>
									{option.name} ({option.width}x{option.height})
								</option>
							{/each}
						</select>
						<button
							class="btn btn-secondary"
							onclick={() => bleConnectionStore.queryDisplayInfo()}
							disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
						>
							Query device
						</button>
					</div>
					<!-- LED flashing control (moved here) -->
					<div class="flex flex-wrap gap-3">
						<h4 class="w-full">LED flashing</h4>
						<button
							class="btn btn-error"
							onclick={() => bleConnectionStore.sendRxTxCommand('e300')}
							disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
						>
							Disable
						</button>
						<button
							class="btn btn-success"
							onclick={() => bleConnectionStore.sendRxTxCommand('e301')}
							disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
						>
							Enable
						</button>
					</div>
					<div class="flex flex-wrap gap-3">
						<h4 class="w-full">LED rainbow</h4>
						<button
							class="btn btn-secondary"
							onclick={() => bleConnectionStore.sendRxTxCommand('e401')}
							disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
						>
							Start rainbow
						</button>
						<button
							class="btn"
							onclick={() => bleConnectionStore.sendRxTxCommand('e400')}
							disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
						>
							Stop rainbow
						</button>
					</div>
					<div class="flex flex-wrap gap-3">
						<h4 class="w-full">Screen refresh</h4>
						<button
							class="btn btn-primary"
							onclick={() => bleConnectionStore.sendRxTxCommand('e200')}
							disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
						>
							Flush (partial)
						</button>
					</div>
					<div class="flex flex-wrap gap-3">
						<h4 class="w-full">Scene</h4>
						<button
							class="btn btn-primary"
							onclick={() => bleConnectionStore.sendRxTxCommand('e1' + hb(0))}
							disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
						>
							0: Image mode (no scene)
						</button>
						<button
							class="btn btn-secondary"
							onclick={() => bleConnectionStore.sendRxTxCommand('e1' + hb(1))}
							disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
						>
							1: Default
						</button>
						<button
							class="btn btn-accent"
							onclick={() => bleConnectionStore.sendRxTxCommand('e1' + hb(2))}
							disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
						>
							2: Time + Date
						</button>
					</div>

					<div class="flex flex-wrap gap-3">
						<h4 class="w-full">Fill byte (hex)</h4>
						<div class="join">
							<input
								class="input input-primary input-bordered join-item w-24"
								bind:value={charByteHex}
								disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
							/>
							<button
								class="btn btn-primary join-item"
								onclick={() => bleConnectionStore.sendRxTxCommand('b1' + (charByteHex || '00'))}
								disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
							>
								Draw
							</button>
						</div>
					</div>
				</div>
			</div>
		</div>

		<!-- Image upload & dithering -->
		<div class="collapse collapse-arrow bg-base-300 rounded-lg">
			<input type="radio" name="accordion" />
			<div class="collapse-title text-lg font-semibold">Image upload</div>
			<div class="collapse-content">
				<div class="flex flex-col gap-3">
					<div class="flex flex-col lg:flex-row flex-wrap gap-3">
						<fieldset class="fieldset p-0">
							<legend class="fieldset-legend">Choose image</legend>
							<input
								class="file-input file-input-bordered file-input-primary"
								type="file"
								multiple
								accept=".png,.jpg,.jpeg,.bmp,.webp"
								onchange={handleImageFile}
								disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
							/>
						</fieldset>

						{#if photoItems.length > 1}
							<fieldset class="fieldset p-0">
								<legend class="fieldset-legend">Slideshow interval</legend>
								<input
									type="number"
									min="1"
									class="input input-bordered input-primary w-40"
									bind:value={slideshowIntervalSeconds}
									disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
								/>
							</fieldset>
						{/if}

						{#if photoItems.length > 1}
							<fieldset class="fieldset p-0">
								<legend class="fieldset-legend">Editing photo</legend>
								<div class="join">
									<button
										class="btn join-item"
										type="button"
										onclick={() => switchSelectedPhoto(-1)}
									>
										⬅️
									</button>
									<div
										class="join-item flex items-center px-3 border border-base-300 bg-base-100 min-w-48 text-sm"
									>
										{selectedPhotoIndex + 1} / {photoItems.length}: {getSelectedPhoto()?.name}
									</div>
									<button
										class="btn join-item"
										type="button"
										onclick={() => switchSelectedPhoto(1)}
									>
										➡️
									</button>
								</div>
							</fieldset>
						{/if}

						<fieldset class="fieldset p-0">
							<legend class="fieldset-legend">Dithering</legend>
							<select
								bind:value={ditheringMode}
								class="select select-bordered select-primary w-60"
								onchange={applyDithering}
								disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
							>
								<optgroup label="BW">
									<option value="bw_FloydSteinberg">BW FloydSteinberg</option>
									<option value="bw_FalseFloydSteinberg">BW FalseFloydSteinberg</option>
									<option value="bw_Stucki">BW Stucki</option>
									<option value="bw_Atkinson">BW Atkinson</option>
									<option value="bw_Jarvis">BW Jarvis</option>
									<option value="bw_Burkes">BW Burkes</option>
									<option value="bw_Sierra">BW Sierra</option>
									<option value="bw_TwoSierra">BW TwoSierra</option>
									<option value="bw_SierraLite">BW SierraLite</option>
								</optgroup>
								<optgroup label="BWR">
									<option value="bwr_FloydSteinberg">BWR FloydSteinberg</option>
									<option value="bwr_FalseFloydSteinberg">BWR FalseFloydSteinberg</option>
									<option value="bwr_Stucki">BWR Stucki</option>
									<option value="bwr_Atkinson">BWR Atkinson</option>
									<option value="bwr_Jarvis">BWR Jarvis</option>
									<option value="bwr_Burkes">BWR Burkes</option>
									<option value="bwr_Sierra">BWR Sierra</option>
									<option value="bwr_TwoSierra">BWR TwoSierra</option>
									<option value="bwr_SierraLite">BWR SierraLite</option>
								</optgroup>
							</select>
						</fieldset>

						<fieldset class="fieldset p-0">
							<legend class="fieldset-legend">Fit / crop</legend>
							<select
								class="select select-bordered select-primary w-60"
								value={getSelectedPhoto()?.imageFitMode ?? 'contain'}
								onchange={handleImageFitModeChange}
								disabled={imageControlsDisabled()}
							>
								<option value="contain">Fit with padding</option>
								<option value="cover">Fill and crop</option>
								<option value="stretch">Stretch to display</option>
							</select>
						</fieldset>

						<fieldset class="fieldset p-0">
							<legend class="fieldset-legend">Crop position</legend>
							<div class="flex flex-col gap-2">
								<div class="grid grid-cols-3 gap-2 w-40">
									<button
										type="button"
										class="btn btn-sm btn-outline"
										disabled={imageControlsDisabled()}
										onclick={() => rotateImage(-90)}
										title="Rotate left"
									>
										↺
									</button>
									<button
										type="button"
										class="btn btn-sm btn-outline"
										disabled={getSelectedPhoto()?.imageFitMode !== 'cover' ||
											imageControlsDisabled()}
										onclick={() => nudgeCoverCrop(0, -0.15)}
										title="Move crop up"
									>
										⬆️
									</button>
									<button
										type="button"
										class="btn btn-sm btn-outline"
										disabled={imageControlsDisabled()}
										onclick={() => rotateImage(90)}
										title="Rotate right"
									>
										↻
									</button>
									<button
										type="button"
										class="btn btn-sm btn-outline"
										disabled={getSelectedPhoto()?.imageFitMode !== 'cover' ||
											imageControlsDisabled()}
										onclick={() => nudgeCoverCrop(-0.15, 0)}
										title="Move crop left"
									>
										⬅️
									</button>
									<button
										type="button"
										class="btn btn-sm btn-outline"
										disabled={getSelectedPhoto()?.imageFitMode !== 'cover' ||
											imageControlsDisabled()}
										onclick={resetCoverCropPosition}
										title="Center crop"
									>
										🎯
									</button>
									<button
										type="button"
										class="btn btn-sm btn-outline"
										disabled={getSelectedPhoto()?.imageFitMode !== 'cover' ||
											imageControlsDisabled()}
										onclick={() => nudgeCoverCrop(0.15, 0)}
										title="Move crop right"
									>
										➡️
									</button>
									<div class="flex items-center justify-center text-xs text-base-content/70">
										{getSelectedPhoto()?.imageRotation ?? 0}°
									</div>
									<button
										type="button"
										class="btn btn-sm btn-outline"
										disabled={getSelectedPhoto()?.imageFitMode !== 'cover' ||
											imageControlsDisabled()}
										onclick={() => nudgeCoverCrop(0, 0.15)}
										title="Move crop down"
									>
										⬇️
									</button>
									<div></div>
								</div>
								<div class="text-xs text-base-content/70">
									X: {Math.round((getSelectedPhoto()?.coverPanX ?? 0) * 100)}% Y: {Math.round(
										(getSelectedPhoto()?.coverPanY ?? 0) * 100
									)}%
								</div>
							</div>
						</fieldset>

						<fieldset class="fieldset p-0">
							<legend class="fieldset-legend">Pixel-art resize</legend>
							<label class="cursor-pointer inline-flex items-center gap-2">
								<input
									type="checkbox"
									class="checkbox checkbox-primary"
									checked={getSelectedPhoto()?.usePixelArtResize ?? false}
									disabled={imageControlsDisabled()}
									onchange={togglePixelArtResize}
								/>
								<span class="label-text">Use PixelOE-style contrast resize</span>
							</label>
						</fieldset>

						<fieldset class="fieldset p-0">
							<legend class="fieldset-legend">Serpentine</legend>
							<label class="cursor-pointer inline-flex items-center gap-2">
								<input
									type="checkbox"
									class="checkbox checkbox-primary"
									bind:checked={serpentine}
									disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
									onchange={() => void applyDithering()}
								/>
								<span class="label-text">Enable</span>
							</label>
						</fieldset>
					</div>

					<div class="flex flex-col gap-3">
						<div class="self-start bg-base-100 rounded-lg p-3 w-full max-w-[500px]">
							<div class="mb-2 text-sm text-base-content/70">
								Canvas: {bleConnectionStore.displayWidth}x{bleConnectionStore.displayHeight}
								{#if getSelectedPhoto()}
									| Editing: {getSelectedPhoto()?.name}
								{/if}
							</div>
							<canvas
								use:onCanvasReady
								width={bleConnectionStore.displayWidth}
								height={bleConnectionStore.displayHeight}
								class="border w-full"
								style="image-rendering: pixelated"
							></canvas>
						</div>
						<button
							disabled={!bleConnectionStore.connected ||
								bleConnectionStore.isFlashingFirmware ||
								!photoItems.length}
							class="btn btn-primary self-start"
							onclick={uploadImage}
						>
							{photoItems.length > 1 ? 'Upload Slideshow' : 'Upload Photo'}
						</button>
					</div>
				</div>
			</div>
		</div>

		<!-- Firmware flashing -->
		<div class="collapse collapse-arrow bg-base-300 rounded-lg">
			<input type="radio" name="accordion" />
			<div class="collapse-title text-lg font-semibold">Flash firmware</div>
			<div class="collapse-content">
				<div class="flex flex-col gap-3">
					<div class="flex flex-col lg:flex-row flex-wrap gap-3">
						<fieldset class="fieldset p-0">
							<legend class="fieldset-legend">Select firmware</legend>
							<input
								class="file-input file-input-primary file-input-bordered file-input-sm"
								type="file"
								accept=".bin"
								onchange={handleFirmwareFile}
								disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
								onclick={resetFileInputValue}
							/>
						</fieldset>
					</div>

					<div class="flex flex-col gap-3">
						<textarea
							name="firmware"
							id="firmware"
							class="textarea textarea-primary w-full resize-none focus:outline-none active:outline-none select-none pointer-events-none"
							bind:value={firmwareArray}
						></textarea>
						<div class="flex gap-3 items-center">
							<progress
								class="progress progress-primary w-full h-6 rounded-lg"
								value={bleConnectionStore.firmwareUploadProgress}
								max="100"
							></progress>
							<div>{Math.ceil(bleConnectionStore.firmwareUploadProgress)}%</div>
						</div>
						<!-- {#if !bleConnectionStore.isFlashingFirmware && bleConnectionStore.firmwareUploadProgress >= 99}
							<div role="alert" class="alert alert-success">
								<svg
									xmlns="http://www.w3.org/2000/svg"
									class="h-6 w-6 shrink-0 stroke-current"
									fill="none"
									viewBox="0 0 24 24"
								>
									<path
										stroke-linecap="round"
										stroke-linejoin="round"
										stroke-width="2"
										d="M9 12l2 2 4-4m6 2a9 9 0 11-18 0 9 9 0 0118 0z"
									/>
								</svg>
								<span>Firmware flashing complete!</span>
							</div>
						{/if} -->
						<button
							disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
							class="btn btn-primary self-start"
							onclick={() => bleConnectionStore.flashFirmware(0x20000, firmwareArray)}
						>
							Upload Firmware
						</button>
					</div>
				</div>
			</div>
		</div>

		<!-- <div class="collapse collapse-arrow bg-base-300 rounded-lg">
			<input type="radio" name="accordion" checked />
			<div class="collapse-title text-lg font-semibold">Weather</div>
			<div class="collapse-content">
				<div class="prose max-w-none p-0">
					<div class="flex flex-col lg:flex-row gap-3">
						<fieldset class="fieldset p-0">
							<legend class="fieldset-legend">Location</legend>
							<input
								class="input input-primary"
								type="text"
								bind:value={location}
								disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
							/>
						</fieldset>
						<fieldset class="fieldset p-0">
							<legend class="fieldset-legend hidden lg:opacity-0">a</legend>
							<button
								disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
								class="btn btn-primary"
								onclick={() => weatherStore.getForecast(location)}
							>
								Get forecast
							</button>
						</fieldset>
					</div>
					<div id="capture" class="w-[250px] h-[128px] flex pb-[6px] bg-white"></div>
				</div>
			</div>
		</div> -->
	</div>
	<div
		class="rounded-lg bg-base-300 p-3 w-full lg:w-2/5 max-h-[50vh] flex flex-col overflow-auto shrink-0 gap-3"
	>
		<div class="flex items-center prose max-w-none">
			<h4 class="flex-grow !m-0">Logs</h4>
			<button class="btn btn-sm btn-accent" onclick={() => logStore.clearLogs()}>Clear</button>
		</div>
		<div
			class="flex flex-grow flex-col-reverse overflow-auto text-lime-400 text-xs min-h-90 bg-black w-full rounded-lg p-3"
		>
			{#each logStore.logs as log, index (`${index}-${log}`)}
				<pre><code>{log}</code></pre>
			{/each}
		</div>
	</div>
</div>
