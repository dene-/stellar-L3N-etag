<script lang="ts">
	import {
		bleConnectionStore,
		DISPLAY_MODEL_OPTIONS,
		SLIDESHOW_INTERVALS
	} from '../../stores/connectionStore.svelte';
	import Toggle from '../ui/Toggle.svelte';

	const store = bleConnectionStore;
	let patternHex = $state('ff');
	let patternValid = $derived(/^[0-9a-f]{1,2}$/i.test(patternHex.trim()));

	// Firmware before v0.9.0 does not answer E7.
	let clockSupported = $derived(store.clockIntervalMinutes !== null);
	const CLOCK_INTERVALS = [1, 2, 5, 10, 15, 30, 60];

	// Controls follow the screen on the tag; firmware before v0.10.0 doesn't say which, so all show.
	let showsClock = $derived(
		store.activeScene === null || store.activeScene === 1 || store.activeScene === 2
	);
	let showsImages = $derived(store.activeScene === 0 || store.activeScene === 3);
	// Firmware before v0.14.0 does not answer E9.
	let imagesKnown = $derived(store.storedImageCount !== null);
	let intervalOptions = $derived(
		store.slideshowIntervalSeconds === null ||
			SLIDESHOW_INTERVALS.some((option) => option.value === store.slideshowIntervalSeconds)
			? SLIDESHOW_INTERVALS
			: [
					...SLIDESHOW_INTERVALS,
					{ value: store.slideshowIntervalSeconds, label: `${store.slideshowIntervalSeconds} s` }
				].sort((a, b) => a.value - b.value)
	);

	function picturesHint() {
		if (!imagesKnown) return 'Needs firmware v0.14.0 to show what is stored';
		if (!store.storedImageCount) return 'None stored yet';
		return store.storedImageCount === 1
			? '1 stored on the tag'
			: `${store.storedImageCount} stored on the tag`;
	}

	function modelLabel(option: (typeof DISPLAY_MODEL_OPTIONS)[number]) {
		if (option.model !== 0) return `${option.name} · ${option.width}×${option.height}`;
		return store.selectedModel === 0 ? `Auto detect · ${store.deviceModelName}` : 'Auto detect';
	}
</script>

<div class="flex flex-col divide-y divide-line">
	<label class="flex min-h-14 flex-wrap items-center justify-between gap-x-6 gap-y-2 py-3">
		<span class="flex flex-col">
			<span class="text-[0.95rem]">Display</span>
			<span class="text-sm text-muted">Saved on the tag</span>
		</span>
		<select
			class="field w-auto min-w-56"
			value={store.selectedModel}
			disabled={store.commandsBlocked}
			onchange={(event) => store.setDisplayModel(Number(event.currentTarget.value))}
		>
			{#each DISPLAY_MODEL_OPTIONS as option (option.model)}
				<option value={option.model}>{modelLabel(option)}</option>
			{/each}
		</select>
	</label>

	{#if showsClock}
		<Toggle
			label="Fast refresh"
			hint={store.fastRefreshSupported
				? 'Fewer full refreshes on the clock screens; more ghosting'
				: 'Not supported by this display'}
			checked={store.fastRefreshEnabled}
			disabled={store.commandsBlocked || !store.fastRefreshSupported}
			onchange={(enabled) => store.setFastRefreshEnabled(enabled)}
		/>

		<label class="flex min-h-14 flex-wrap items-center justify-between gap-x-6 gap-y-2 py-3">
			<span class="flex flex-col">
				<span class="text-[0.95rem]">Clock refresh</span>
				<span class="text-sm text-muted">
					{clockSupported ? 'How often the clock screens show a new time' : 'Needs firmware v0.9.0'}
				</span>
			</span>
			<select
				class="field w-auto min-w-44"
				value={store.clockIntervalMinutes ?? 1}
				disabled={store.commandsBlocked || !clockSupported}
				onchange={(event) =>
					store.setClockSchedule(Number(event.currentTarget.value), store.clockSync === true)}
			>
				{#each CLOCK_INTERVALS as minutes (minutes)}
					<option value={minutes}
						>{minutes === 1 ? 'Every minute' : `Every ${minutes} minutes`}</option
					>
				{/each}
			</select>
		</label>

		<Toggle
			label="Finish on the minute"
			hint="Starts each refresh early, so the new time appears as the minute changes"
			checked={store.clockSync}
			disabled={store.commandsBlocked || !clockSupported}
			onchange={(enabled) => store.setClockSchedule(store.clockIntervalMinutes ?? 1, enabled)}
		/>
	{/if}

	{#if showsImages}
		<div class="flex min-h-14 flex-wrap items-center justify-between gap-x-6 gap-y-2 py-3">
			<span class="flex flex-col">
				<span class="text-[0.95rem]">Pictures</span>
				<span class="text-sm text-muted">{picturesHint()}</span>
			</span>
			<a class="btn btn-outline" href="#images">Send pictures</a>
		</div>
	{/if}

	{#if store.activeScene === 3}
		<label class="flex min-h-14 flex-wrap items-center justify-between gap-x-6 gap-y-2 py-3">
			<span class="flex flex-col">
				<span class="text-[0.95rem]">Next picture every</span>
				<span class="text-sm text-muted">
					{imagesKnown ? 'Saved with the pictures on the tag' : 'Needs firmware v0.14.0'}
				</span>
			</span>
			<select
				class="field w-auto min-w-44"
				value={store.slideshowIntervalSeconds ?? 60}
				disabled={store.commandsBlocked || !imagesKnown || !store.storedImageCount}
				onchange={(event) => store.setSlideshowInterval(Number(event.currentTarget.value))}
			>
				{#each intervalOptions as option (option.value)}
					<option value={option.value}>{option.label}</option>
				{/each}
			</select>
		</label>
	{/if}

	<Toggle
		label="Status light"
		hint="Blinks now and then to show the tag is alive"
		checked={store.ledFlashingEnabled}
		disabled={store.commandsBlocked}
		onchange={(enabled) => store.setLedFlashing(enabled)}
	/>

	<div class="flex min-h-14 flex-wrap items-center justify-between gap-x-6 gap-y-2 py-3">
		<span class="flex flex-col">
			<span class="text-[0.95rem]">Screen</span>
			<span class="text-sm text-muted">Draw it again with a full refresh</span>
		</span>
		<button class="btn btn-outline" disabled={store.commandsBlocked} onclick={() => store.redraw()}
			>Redraw</button
		>
	</div>

	<div class="flex min-h-14 flex-wrap items-center justify-between gap-x-6 gap-y-2 py-3">
		<span class="flex flex-col">
			<span class="text-[0.95rem]">Clock</span>
			<span class="text-sm text-muted">Set on every connect, with your time zone</span>
		</span>
		<button
			class="btn btn-outline"
			disabled={store.commandsBlocked}
			onclick={() => store.syncTime()}>Set now</button
		>
	</div>

	<details class="group py-3">
		<summary
			class="flex min-h-11 cursor-pointer list-none items-center justify-between text-soft hover:text-fg"
		>
			<span class="text-[0.95rem]">Tools</span>
			<span class="eyebrow transition-transform group-open:rotate-45" aria-hidden="true">+</span>
		</summary>
		<div class="flex flex-col gap-5 pt-4">
			<div class="flex flex-wrap items-center gap-3">
				<button
					class="btn btn-outline"
					disabled={store.commandsBlocked}
					onclick={() => store.playLedRainbow(true)}>Play rainbow</button
				>
				<button
					class="btn btn-text"
					disabled={store.commandsBlocked}
					onclick={() => store.playLedRainbow(false)}>Stop</button
				>
			</div>
			<div class="flex flex-wrap items-center gap-3">
				<button
					class="btn btn-outline"
					disabled={store.commandsBlocked}
					onclick={() => store.requestTemperature()}>Read temperature</button
				>
				<button
					class="btn btn-outline"
					disabled={store.commandsBlocked}
					onclick={() => store.queryDisplayInfo()}>Read display info</button
				>
			</div>
			<form
				class="flex flex-wrap items-center gap-3"
				onsubmit={(event) => {
					event.preventDefault();
					if (patternValid) store.drawPattern(parseInt(patternHex, 16));
				}}
			>
				<label class="flex items-center gap-3">
					<span class="text-sm text-soft">Test pattern byte</span>
					<input
						class="field w-20 font-mono uppercase"
						maxlength="2"
						spellcheck="false"
						aria-invalid={!patternValid}
						bind:value={patternHex}
					/>
				</label>
				<button class="btn btn-outline" disabled={store.commandsBlocked || !patternValid}
					>Draw</button
				>
			</form>
		</div>
	</details>
</div>
