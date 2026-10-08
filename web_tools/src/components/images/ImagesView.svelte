<script lang="ts">
	import type { ImageFitMode, ImageRotation } from '#lib/pixel-art-resize.ts';
	import {
		clampPanOffset,
		computeMaxImageCount,
		loadPhotoItem,
		revokePhotoUrls,
		type PhotoItem
	} from '#lib/photo-utils.ts';
	import { renderAndBuildBuffers, renderPhotoToCanvas } from '#lib/rendering.ts';
	import { bleConnectionStore, SLIDESHOW_INTERVALS } from '../../stores/connectionStore.svelte';
	import { logStore } from '../../stores/logStore.svelte';
	import FileDrop from '../ui/FileDrop.svelte';
	import Icon from '../ui/Icon.svelte';
	import Segmented from '../ui/Segmented.svelte';
	import Toggle from '../ui/Toggle.svelte';

	const store = bleConnectionStore;
	const ACCEPT = '.png,.jpg,.jpeg,.bmp,.webp,.gif';
	const ALGORITHMS = [
		['Atkinson', 'Atkinson'],
		['FloydSteinberg', 'Floyd–Steinberg'],
		['FalseFloydSteinberg', 'False Floyd–Steinberg'],
		['Stucki', 'Stucki'],
		['Jarvis', 'Jarvis'],
		['Burkes', 'Burkes'],
		['Sierra', 'Sierra'],
		['TwoSierra', 'Two-row Sierra'],
		['SierraLite', 'Sierra Lite']
	] as const;

	let photos = $state<PhotoItem[]>([]);
	let selected = $state(0);
	let algorithm = $state('Atkinson');
	let serpentine = $state(false);
	let paletteChoice = $state<'bw' | 'bwr' | null>(null); // null: follow the display
	let intervalSeconds = $state(60);
	let canvas: HTMLCanvasElement | undefined = $state();

	let palette = $derived(store.displayHasRed ? (paletteChoice ?? 'bwr') : 'bw');
	let maxImages = $derived(computeMaxImageCount(store.displayWidth, store.displayHeight));
	let photo = $derived(photos[selected]);
	let options = $derived({ mode: `${palette}_${algorithm}`, serpentine });

	// Renders the selected photo whenever it or a setting changes. Each render draws offscreen and
	// only the latest one reaches the preview.
	let renderToken = 0;
	$effect(() => {
		const target = canvas;
		const width = store.displayWidth;
		const height = store.displayHeight;
		const current = photo;
		const dithering = options;
		if (!target) return;
		if (current) {
			// read the per-photo settings here so changing them re-renders
			void [current.imageFitMode, current.imageRotation, current.coverPanX, current.coverPanY];
			void current.usePixelArtResize;
		}
		const token = ++renderToken;
		target.width = width;
		target.height = height;
		const ctx = target.getContext('2d');
		if (!current || !ctx) return;
		const offscreen = document.createElement('canvas');
		offscreen.width = width;
		offscreen.height = height;
		renderPhotoToCanvas(current, offscreen, dithering)
			.then(() => {
				if (token === renderToken) ctx.drawImage(offscreen, 0, 0);
			})
			.catch((error) => logStore.addLog(`Preview failed: ${error}`));
	});

	async function addFiles(files: File[]) {
		const room = maxImages - photos.length;
		if (room <= 0) {
			logStore.addLog(`This display holds at most ${maxImages} pictures.`);
			return;
		}
		if (files.length > room) {
			logStore.addLog(
				`Only the first ${room} pictures were added; the display holds ${maxImages}.`
			);
		}
		const loaded = await Promise.allSettled(
			files.slice(0, room).map((file, index) => loadPhotoItem(file, photos.length + index))
		);
		for (const result of loaded) {
			if (result.status === 'fulfilled') photos.push(result.value);
			else logStore.addLog(String(result.reason));
		}
		selected = Math.max(0, photos.length - 1);
	}

	function remove(index: number) {
		revokePhotoUrls(photos.splice(index, 1));
		selected = Math.min(selected, Math.max(0, photos.length - 1));
	}

	function step(delta: number) {
		selected = (selected + delta + photos.length) % photos.length;
	}

	function nudge(dx: number, dy: number) {
		if (!photo) return;
		photo.coverPanX = clampPanOffset(photo.coverPanX + dx);
		photo.coverPanY = clampPanOffset(photo.coverPanY + dy);
	}

	function rotate(delta: 90 | -90) {
		if (photo) photo.imageRotation = ((photo.imageRotation + delta + 360) % 360) as ImageRotation;
	}

	async function send() {
		if (!store.connected) {
			await store.preConnect();
			return;
		}
		try {
			const rendered = [];
			for (const item of photos) {
				rendered.push(
					await renderAndBuildBuffers(item, store.displayWidth, store.displayHeight, options)
				);
			}
			await store.uploadImageSet(rendered, photos.length > 1 ? intervalSeconds : 0);
		} catch (error) {
			logStore.addLog('Upload error: ' + (error instanceof Error ? error.message : String(error)));
		}
	}
</script>

<div class="grid items-start gap-12 lg:grid-cols-[minmax(0,1fr)_22rem] lg:gap-16">
	<section class="flex min-w-0 flex-col gap-8">
		<div class="flex flex-col gap-4">
			<p class="eyebrow">Images</p>
			<h1 class="display text-4xl sm:text-5xl">Pictures for the tag.</h1>
			<p class="max-w-xl text-soft">
				Converted for the {store.deviceModelName} display ({store.displayWidth}×{store.displayHeight}).
				Add one picture, or up to {maxImages} for a slideshow.
			</p>
		</div>

		{#if photos.length === 0}
			<FileDrop accept={ACCEPT} multiple class="aspect-[296/128] w-full px-6" onfiles={addFiles}>
				{#snippet children({ dragging })}
					<Icon name="upload" size={28} class="mb-4 text-accent" />
					<span class="text-lg">{dragging ? 'Drop to add' : 'Drop pictures here'}</span>
					<span class="mt-1 text-sm text-muted">or click to browse · PNG, JPEG, WebP, BMP, GIF</span
					>
				{/snippet}
			</FileDrop>
		{:else}
			<div
				class="rounded-[1.25rem] bg-paper p-3 shadow-[0_30px_60px_-24px_rgb(0_0_0/0.8)] sm:p-4"
				style="max-width: {Math.max(store.displayWidth * 2.4, 320)}px"
			>
				<canvas
					bind:this={canvas}
					class="block h-auto w-full [image-rendering:pixelated]"
					aria-label="Preview of {photo?.name}"
				></canvas>
			</div>

			<div class="flex items-center gap-4">
				<p class="min-w-0 flex-1 truncate text-sm text-soft">
					<span class="text-fg">{photo?.name}</span>
					<span class="text-muted"> · {selected + 1} of {photos.length}</span>
				</p>
				{#if photos.length > 1}
					<button class="icon-btn" aria-label="Previous picture" onclick={() => step(-1)}>
						<Icon name="chevron-left" />
					</button>
					<button
						class="icon-btn icon-btn-accent"
						aria-label="Next picture"
						onclick={() => step(1)}
					>
						<Icon name="chevron-right" />
					</button>
				{/if}
			</div>

			<ul class="flex flex-wrap gap-3" aria-label="Pictures">
				{#each photos as item, index (item.id)}
					<li class="group relative">
						<button
							class="block size-20 overflow-hidden rounded-lg border-2 transition-colors {index ===
							selected
								? 'border-accent'
								: 'border-transparent hover:border-line-strong'}"
							aria-label="Edit {item.name}"
							aria-pressed={index === selected}
							onclick={() => (selected = index)}
						>
							<img src={item.image.src} alt="" class="size-full object-cover" />
						</button>
						<button
							class="absolute -top-2 -right-2 grid size-6 place-items-center rounded-full bg-fg text-ink opacity-0 transition-opacity group-hover:opacity-100 focus-visible:opacity-100"
							aria-label="Remove {item.name}"
							onclick={() => remove(index)}
						>
							<Icon name="close" size={12} />
						</button>
					</li>
				{/each}
				{#if photos.length < maxImages}
					<li>
						<FileDrop accept={ACCEPT} multiple class="size-20 text-muted" onfiles={addFiles}>
							{#snippet children({ dragging })}
								<span
									class="text-2xl leading-none {dragging ? 'text-accent' : ''}"
									aria-hidden="true">+</span
								>
								<span class="sr-only">Add pictures</span>
							{/snippet}
						</FileDrop>
					</li>
				{/if}
			</ul>
		{/if}
	</section>

	<aside class="flex flex-col gap-8 lg:sticky lg:top-8" aria-label="Conversion">
		<div class="flex flex-col gap-3">
			<span class="eyebrow">Colors</span>
			<Segmented
				label="Colors"
				options={[
					{ value: 'bw', label: 'Black & white' },
					{ value: 'bwr', label: '+ Red' }
				]}
				value={palette}
				disabled={!store.displayHasRed}
				onchange={(value) => (paletteChoice = value)}
			/>
			{#if !store.displayHasRed}
				<span class="text-sm text-muted">This display has no red.</span>
			{/if}
		</div>

		<label class="flex flex-col gap-3">
			<span class="eyebrow">Dithering</span>
			<select class="field" bind:value={algorithm}>
				{#each ALGORITHMS as [value, label] (value)}
					<option {value}>{label}</option>
				{/each}
			</select>
		</label>

		{#if photo}
			<div class="flex flex-col gap-3">
				<span class="eyebrow">Fit</span>
				<Segmented
					label="Fit"
					options={[
						{ value: 'cover' as ImageFitMode, label: 'Fill' },
						{ value: 'contain' as ImageFitMode, label: 'Fit' },
						{ value: 'stretch' as ImageFitMode, label: 'Stretch' }
					]}
					value={photo.imageFitMode}
					onchange={(value) => photo && (photo.imageFitMode = value)}
				/>
			</div>

			<div class="flex flex-col gap-3">
				<span class="eyebrow">Position</span>
				<div class="flex items-center gap-6">
					<div class="grid grid-cols-3 gap-1.5" role="group" aria-label="Move the crop">
						{#snippet pad(
							icon: 'arrow-up' | 'arrow-down' | 'arrow-left' | 'arrow-right' | 'center',
							label: string,
							action: () => void
						)}
							<button
								class="icon-btn size-10!"
								aria-label={label}
								disabled={photo.imageFitMode !== 'cover'}
								onclick={action}><Icon name={icon} size={16} /></button
							>
						{/snippet}
						<span></span>
						{@render pad('arrow-up', 'Move up', () => nudge(0, -0.15))}
						<span></span>
						{@render pad('arrow-left', 'Move left', () => nudge(-0.15, 0))}
						{@render pad(
							'center',
							'Center',
							() => photo && ((photo.coverPanX = 0), (photo.coverPanY = 0))
						)}
						{@render pad('arrow-right', 'Move right', () => nudge(0.15, 0))}
						<span></span>
						{@render pad('arrow-down', 'Move down', () => nudge(0, 0.15))}
						<span></span>
					</div>
					<div class="flex flex-col items-center gap-2">
						<div class="flex gap-1.5">
							<button class="icon-btn size-10!" aria-label="Rotate left" onclick={() => rotate(-90)}
								><Icon name="rotate-left" size={16} /></button
							>
							<button class="icon-btn size-10!" aria-label="Rotate right" onclick={() => rotate(90)}
								><Icon name="rotate-right" size={16} /></button
							>
						</div>
						<span class="text-xs text-muted">{photo.imageRotation}°</span>
					</div>
				</div>
			</div>
		{/if}

		<div class="flex flex-col divide-y divide-line border-y border-line">
			{#if photo}
				<Toggle
					label="Pixel-art resize"
					hint="Keeps edges sharp in drawings"
					checked={photo.usePixelArtResize}
					onchange={(on) => (photo.usePixelArtResize = on)}
				/>
			{/if}
			<Toggle
				label="Serpentine scan"
				hint="Fewer diagonal patterns"
				checked={serpentine}
				onchange={(on) => (serpentine = on)}
			/>
		</div>

		{#if photos.length > 1}
			<div class="flex flex-col gap-3">
				<span class="eyebrow">Next picture every</span>
				<Segmented
					label="Slideshow interval"
					options={SLIDESHOW_INTERVALS}
					value={SLIDESHOW_INTERVALS.some((option) => option.value === intervalSeconds)
						? intervalSeconds
						: null}
					onchange={(value) => (intervalSeconds = value)}
				/>
			</div>
		{/if}

		<div class="flex flex-col gap-3">
			<button
				class="btn btn-primary w-full"
				disabled={store.busy || store.connecting || (store.connected && photos.length === 0)}
				onclick={send}
			>
				{#if !store.connected}
					Connect to send
				{:else if store.isUploadingImages}
					Sending… {Math.ceil(store.imageUploadProgress)}%
				{:else}
					Send {photos.length > 1 ? `${photos.length} pictures` : 'to tag'}
				{/if}
			</button>
			{#if store.isUploadingImages}
				<div
					class="progress"
					role="progressbar"
					aria-valuenow={Math.round(store.imageUploadProgress)}
				>
					<span style="width: {store.imageUploadProgress}%"></span>
				</div>
			{/if}
		</div>
	</aside>
</div>
