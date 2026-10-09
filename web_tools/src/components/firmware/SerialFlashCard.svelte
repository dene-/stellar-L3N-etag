<script lang="ts">
	import type { FirmwareFile } from '#lib/firmware-file.ts';
	import { SERIAL_MAX_FIRMWARE_SIZE } from '#lib/tlsr-serial-flasher.ts';
	import { serialFlashStore } from '../../stores/serialStore.svelte';
	import FirmwareSource from './FirmwareSource.svelte';

	const WIRING = 'https://github.com/dene-/stellar-L3N-etag#first-install-uart';

	const store = serialFlashStore;
	let file = $state<FirmwareFile | null>(null);
</script>

<section class="panel flex flex-col gap-6 p-6 sm:p-8">
	<div class="flex flex-col gap-2">
		<p class="eyebrow">02 · USB serial</p>
		<h2 class="text-2xl">First install over USB</h2>
		<p class="text-soft">
			For a tag still on its original firmware, wired to a USB serial adapter as in the
			<a
				class="text-accent underline-offset-4 hover:underline"
				href={WIRING}
				target="_blank"
				rel="noreferrer">wiring guide</a
			>.
		</p>
	</div>

	{#if !store.supported}
		<p class="border-l-2 border-accent pl-4 text-soft">
			This browser has no Web Serial. Use Chrome or Edge on a computer.
		</p>
	{:else}
		<div class="flex items-center justify-between gap-4">
			<span class="flex items-center gap-2 text-sm text-soft">
				<span
					class="size-2 rounded-full {store.connected ? 'bg-ok' : 'bg-line-strong'}"
					aria-hidden="true"
				></span>
				{store.connected ? 'Serial port open' : 'Serial port closed'}
			</span>
			<button
				class="btn btn-outline"
				disabled={store.busy}
				onclick={() => store.togglePort(file !== null)}
			>
				{store.connected ? 'Close port' : 'Open port'}
			</button>
		</div>

		<FirmwareSource bind:file maxBytes={SERIAL_MAX_FIRMWARE_SIZE} disabled={store.busy} />

		<div class="mt-auto flex flex-col gap-3">
			<div class="flex gap-3">
				<button
					class="btn btn-outline flex-1"
					disabled={!store.connected || store.busy}
					onclick={() => store.unlockFlash()}>Unlock flash</button
				>
				<button
					class="btn btn-primary flex-1"
					disabled={!store.connected || store.busy || !file}
					onclick={() => file && store.writeFirmware(file.data)}>Write firmware</button
				>
			</div>
			{#if store.progress > 0}
				<div class="progress" role="progressbar" aria-valuenow={Math.round(store.progress)}>
					<span style="width: {store.progress}%"></span>
				</div>
			{/if}
			{#if store.busy}
				<p class="text-sm text-muted">Keep this page open and the adapter connected.</p>
			{/if}
			<p class="text-sm text-muted" aria-live="polite">{store.status}</p>
		</div>

		<details class="group border-t border-line pt-3">
			<summary
				class="flex min-h-11 cursor-pointer list-none items-center justify-between text-soft hover:text-fg"
			>
				<span>Advanced</span>
				<span class="eyebrow transition-transform group-open:rotate-45" aria-hidden="true">+</span>
			</summary>
			<div class="flex flex-col gap-5 pt-4">
				<div class="grid grid-cols-2 gap-4">
					<label class="flex flex-col gap-2">
						<span class="eyebrow">Baud rate</span>
						<select
							class="field"
							bind:value={store.baudRate}
							disabled={store.connected || store.busy}
						>
							{#each ['115200', '230400', '460800', '921600', '1500000', '2000000'] as rate (rate)}
								<option value={rate}>{rate}</option>
							{/each}
						</select>
					</label>
					<label class="flex flex-col gap-2">
						<span class="eyebrow">Activation</span>
						<select class="field" bind:value={store.activationMs} disabled={store.busy}>
							{#each [0, 100, 1000, 2000, 3000, 4000, 8000, 16000] as ms (ms)}
								<option value={String(ms)}>{ms < 1000 ? `${ms} ms` : `${ms / 1000} s`}</option>
							{/each}
						</select>
					</label>
				</div>
				<div class="flex flex-wrap gap-3">
					<button
						class="btn btn-outline"
						disabled={!store.connected || store.busy}
						onclick={() => store.eraseAllFlash()}>Erase all flash</button
					>
					<button
						class="btn btn-outline"
						disabled={!store.connected || store.busy}
						onclick={() => store.resetChip()}>Reset chip</button
					>
				</div>
			</div>
		</details>
	{/if}
</section>
