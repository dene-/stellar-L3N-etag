<script lang="ts">
	import type { FirmwareFile } from '#lib/firmware-file.ts';
	import {
		bleConnectionStore,
		OTA_BANK_ADDRESS,
		OTA_MAX_FIRMWARE_SIZE
	} from '../../stores/connectionStore.svelte';
	import FirmwareSource from './FirmwareSource.svelte';
	import FirmwareStatus from './FirmwareStatus.svelte';

	const store = bleConnectionStore;
	let file = $state<FirmwareFile | null>(null);
</script>

<section class="panel flex flex-col gap-6 p-6 sm:p-8">
	<div class="flex flex-col gap-2">
		<p class="eyebrow">01 · Bluetooth</p>
		<h2 class="text-2xl">Update over Bluetooth</h2>
		<p class="text-soft">
			Takes up to a minute. If the upload fails, the tag keeps running its current firmware.
		</p>
		{#if store.connected}
			<FirmwareStatus />
		{/if}
	</div>

	<FirmwareSource
		bind:file
		maxBytes={OTA_MAX_FIRMWARE_SIZE}
		disabled={store.busy}
		installed={store.firmwareVersion}
	/>

	<div class="mt-auto flex flex-col gap-3">
		<button
			class="btn btn-primary"
			disabled={store.busy || store.connecting || (store.connected && !file)}
			onclick={() =>
				store.connected
					? file && store.flashFirmware(OTA_BANK_ADDRESS, file.data)
					: store.preConnect()}
		>
			{#if !store.connected}
				Connect to update
			{:else if store.isFlashingFirmware}
				Updating… {Math.ceil(store.firmwareUploadProgress)}%
			{:else if file}
				Install {file.name}
			{:else}
				Update firmware
			{/if}
		</button>
		{#if store.isFlashingFirmware}
			<div
				class="progress"
				role="progressbar"
				aria-valuenow={Math.round(store.firmwareUploadProgress)}
			>
				<span style="width: {store.firmwareUploadProgress}%"></span>
			</div>
			<p class="text-sm text-muted">Keep this page open and the tag close by.</p>
		{/if}
	</div>
</section>
