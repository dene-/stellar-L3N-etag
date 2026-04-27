<script lang="ts">
	import { bleConnectionStore } from '../stores/connectionStore.svelte';
	import { logStore } from '../stores/logStore.svelte';
	import type { ImageFitMode, ImageRotation } from '$lib/pixel-art-resize';
	import {
		renderPhotoToCanvas,
		renderAndBuildBuffers,
		type DitheringOptions
	} from '$lib/rendering';
	import {
		loadPhotoItem,
		revokePhotoUrls,
		clampPanOffset,
		computeMaxImageCount,
		type PhotoItem
	} from '$lib/photo-utils';

	let maxImages = $derived(
		computeMaxImageCount(bleConnectionStore.displayWidth, bleConnectionStore.displayHeight)
	);

	let ditheringMode: string = $state('bwr_Atkinson');
	let serpentine = $state(false);
	let photoItems = $state<PhotoItem[]>([]);
	let selectedPhotoIndex = $state(0);
	let slideshowIntervalSeconds = $state(60);
	let canvasEl: HTMLCanvasElement | null = null;

	function getDitheringOptions(): DitheringOptions {
		return { mode: ditheringMode, serpentine };
	}

	function getSelectedPhoto(): PhotoItem | null {
		return photoItems[selectedPhotoIndex] ?? null;
	}

	function imageControlsDisabled(): boolean {
		return (
			!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware || !getSelectedPhoto()
		);
	}

	function onCanvasReady(node: HTMLCanvasElement) {
		canvasEl = node;
		const ctx = node.getContext('2d', { willReadFrequently: true });
		if (ctx) {
			ctx.fillStyle = '#fff';
			ctx.fillRect(0, 0, node.width, node.height);
		}
		return {
			destroy() {
				canvasEl = null;
			}
		};
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
		await renderPhotoToCanvas(selectedPhoto, canvasEl, getDitheringOptions());
	}

	$effect(() => {
		if (!canvasEl) return;
		const targetWidth = bleConnectionStore.displayWidth;
		const targetHeight = bleConnectionStore.displayHeight;
		canvasEl.width = targetWidth;
		canvasEl.height = targetHeight;
		void renderSelectedPhoto();
	});

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
			console.error(error);
			logStore.addLog('Error: ' + (error instanceof Error ? error.message : String(error)));
		}
	}

	function switchSelectedPhoto(delta: number) {
		if (!photoItems.length) return;
		selectedPhotoIndex = (selectedPhotoIndex + delta + photoItems.length) % photoItems.length;
		void renderSelectedPhoto();
	}

	function handleImageFitModeChange(event: Event) {
		const select = event.currentTarget;
		if (!(select instanceof HTMLSelectElement)) return;
		const selectedPhoto = getSelectedPhoto();
		if (!selectedPhoto) return;
		selectedPhoto.imageFitMode = select.value as ImageFitMode;
		void renderSelectedPhoto();
	}

	function nudgeCoverCrop(deltaX: number, deltaY: number) {
		const selectedPhoto = getSelectedPhoto();
		if (!selectedPhoto) return;
		selectedPhoto.coverPanX = clampPanOffset(selectedPhoto.coverPanX + deltaX);
		selectedPhoto.coverPanY = clampPanOffset(selectedPhoto.coverPanY + deltaY);
		void renderSelectedPhoto();
	}

	function resetCoverCropPosition() {
		const selectedPhoto = getSelectedPhoto();
		if (!selectedPhoto) return;
		selectedPhoto.coverPanX = 0;
		selectedPhoto.coverPanY = 0;
		void renderSelectedPhoto();
	}

	function rotateImage(delta: 90 | -90) {
		const selectedPhoto = getSelectedPhoto();
		if (!selectedPhoto) return;
		const nextRotation = (selectedPhoto.imageRotation + delta + 360) % 360;
		selectedPhoto.imageRotation = nextRotation as ImageRotation;
		void renderSelectedPhoto();
	}

	function togglePixelArtResize(event: Event) {
		const input = event.currentTarget;
		const selectedPhoto = getSelectedPhoto();
		if (!(input instanceof HTMLInputElement) || !selectedPhoto) return;
		selectedPhoto.usePixelArtResize = input.checked;
		void renderSelectedPhoto();
	}

	async function uploadImage() {
		if (!photoItems.length) return;
		const maxImageCount = computeMaxImageCount(
			bleConnectionStore.displayWidth,
			bleConnectionStore.displayHeight
		);
		if (photoItems.length > maxImageCount) {
			logStore.addLog(
				`Too many photos for device flash. Maximum for this display is ${maxImageCount}.`
			);
			return;
		}
		try {
			const dithering = getDitheringOptions();
			const renderedPhotos = [];
			for (const photo of photoItems) {
				renderedPhotos.push(
					await renderAndBuildBuffers(
						photo,
						bleConnectionStore.displayWidth,
						bleConnectionStore.displayHeight,
						dithering
					)
				);
			}
			await bleConnectionStore.uploadImageSet(
				renderedPhotos,
				photoItems.length > 1 ? slideshowIntervalSeconds : 0
			);
		} catch (error) {
			console.error(error);
			logStore.addLog('Upload error: ' + (error instanceof Error ? error.message : String(error)));
		}
	}
</script>

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
					<div class="text-xs text-base-content/70 mt-1">
						Up to {maxImages} images for this display
					</div>
				</fieldset>

				{#if photoItems.length > 1}
					<fieldset class="fieldset p-0">
						<legend class="fieldset-legend">Slideshow interval</legend>
						<input
							type="number"
							min="1"
							max="65535"
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
							<button class="btn join-item" type="button" onclick={() => switchSelectedPhoto(-1)}
								>⬅️</button
							>
							<div
								class="join-item flex items-center px-3 border border-base-300 bg-base-100 min-w-48 text-sm"
							>
								{selectedPhotoIndex + 1} / {photoItems.length}: {getSelectedPhoto()?.name}
							</div>
							<button class="btn join-item" type="button" onclick={() => switchSelectedPhoto(1)}
								>➡️</button
							>
						</div>
					</fieldset>
				{/if}

				<fieldset class="fieldset p-0">
					<legend class="fieldset-legend">Dithering</legend>
					<select
						bind:value={ditheringMode}
						class="select select-bordered select-primary w-60"
						onchange={() => void renderSelectedPhoto()}
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
						value={getSelectedPhoto()?.imageFitMode ?? 'cover'}
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
								title="Rotate left">↺</button
							>
							<button
								type="button"
								class="btn btn-sm btn-outline"
								disabled={getSelectedPhoto()?.imageFitMode !== 'cover' || imageControlsDisabled()}
								onclick={() => nudgeCoverCrop(0, -0.15)}
								title="Move crop up">⬆️</button
							>
							<button
								type="button"
								class="btn btn-sm btn-outline"
								disabled={imageControlsDisabled()}
								onclick={() => rotateImage(90)}
								title="Rotate right">↻</button
							>
							<button
								type="button"
								class="btn btn-sm btn-outline"
								disabled={getSelectedPhoto()?.imageFitMode !== 'cover' || imageControlsDisabled()}
								onclick={() => nudgeCoverCrop(-0.15, 0)}
								title="Move crop left">⬅️</button
							>
							<button
								type="button"
								class="btn btn-sm btn-outline"
								disabled={getSelectedPhoto()?.imageFitMode !== 'cover' || imageControlsDisabled()}
								onclick={resetCoverCropPosition}
								title="Center crop">🎯</button
							>
							<button
								type="button"
								class="btn btn-sm btn-outline"
								disabled={getSelectedPhoto()?.imageFitMode !== 'cover' || imageControlsDisabled()}
								onclick={() => nudgeCoverCrop(0.15, 0)}
								title="Move crop right">➡️</button
							>
							<div class="flex items-center justify-center text-xs text-base-content/70">
								{getSelectedPhoto()?.imageRotation ?? 0}°
							</div>
							<button
								type="button"
								class="btn btn-sm btn-outline"
								disabled={getSelectedPhoto()?.imageFitMode !== 'cover' || imageControlsDisabled()}
								onclick={() => nudgeCoverCrop(0, 0.15)}
								title="Move crop down">⬇️</button
							>
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
							onchange={() => void renderSelectedPhoto()}
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
				{#if bleConnectionStore.isUploadingImages}
					<div class="flex gap-3 items-center w-full max-w-[500px]">
						<progress
							class="progress progress-primary w-full h-4 rounded-lg"
							value={bleConnectionStore.imageUploadProgress}
							max="100"
						></progress>
						<div class="text-sm whitespace-nowrap">
							{Math.ceil(bleConnectionStore.imageUploadProgress)}%
						</div>
					</div>
				{/if}
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
