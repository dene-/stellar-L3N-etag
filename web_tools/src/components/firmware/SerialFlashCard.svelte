<script lang="ts">
	import { onDestroy } from 'svelte';
	import type { FirmwareFile } from '#lib/firmware-file.ts';
	import { TlsrSerialFlasher } from '#lib/tlsr-serial-flasher.ts';
	import { logStore } from '../../stores/logStore.svelte';
	import FirmwareSource from './FirmwareSource.svelte';

	const WIRING = 'https://github.com/dene-/stellar-L3N-etag#first-install-uart';
	const IDLE_STATUS = 'Open the serial port of your USB adapter to start.';

	const supported = TlsrSerialFlasher.isSupported();
	let file = $state<FirmwareFile | null>(null);
	let connected = $state(false);
	let busy = $state(false);
	let progress = $state(0);
	let status = $state(IDLE_STATUS);
	let baudRate = $state('460800');
	let activationMs = $state('3000');

	const flasher = new TlsrSerialFlasher({
		log: (message: string) => logStore.addLog(message),
		onStatus: (message: string) => (status = message),
		onProgress: (percent: number) => (progress = percent),
		onConnectionChange: (open: boolean) => {
			connected = open;
			if (!open) {
				progress = 0;
				status = IDLE_STATUS;
			}
		}
	});

	onDestroy(() => {
		if (connected) void flasher.close();
	});

	function errorText(error: unknown) {
		return error instanceof Error ? error.message : String(error);
	}

	async function togglePort() {
		try {
			if (connected) {
				await flasher.close();
				return;
			}
			await flasher.open(Number(baudRate));
			status = file
				? 'Ready. Unlock the flash, then write the firmware.'
				: 'Now choose a firmware file.';
		} catch (error) {
			logStore.addLog('Serial port: ' + errorText(error));
			status = 'Could not open the serial port.';
		}
	}

	async function run(action: () => Promise<void>) {
		busy = true;
		try {
			await action();
		} catch (error) {
			logStore.addLog('Serial flashing: ' + errorText(error));
			status = 'That did not work; see the activity log.';
		} finally {
			busy = false;
		}
	}

	const activation = () => Number(activationMs);
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

	{#if !supported}
		<p class="border-l-2 border-accent pl-4 text-soft">
			This browser has no Web Serial. Use Chrome or Edge on a computer.
		</p>
	{:else}
		<div class="flex items-center justify-between gap-4">
			<span class="flex items-center gap-2 text-sm text-soft">
				<span
					class="size-2 rounded-full {connected ? 'bg-ok' : 'bg-line-strong'}"
					aria-hidden="true"
				></span>
				{connected ? 'Serial port open' : 'Serial port closed'}
			</span>
			<button class="btn btn-outline" disabled={busy} onclick={togglePort}>
				{connected ? 'Close port' : 'Open port'}
			</button>
		</div>

		<FirmwareSource bind:file maxBytes={512 * 1024} disabled={busy} />

		<div class="mt-auto flex flex-col gap-3">
			<div class="flex gap-3">
				<button
					class="btn btn-outline flex-1"
					disabled={!connected || busy}
					onclick={() => run(() => flasher.unlockFlash(activation()))}>Unlock flash</button
				>
				<button
					class="btn btn-primary flex-1"
					disabled={!connected || busy || !file}
					onclick={() => file && run(() => flasher.flashFirmware(file!.data, activation()))}
					>Write firmware</button
				>
			</div>
			{#if progress > 0}
				<div class="progress" role="progressbar" aria-valuenow={Math.round(progress)}>
					<span style="width: {progress}%"></span>
				</div>
			{/if}
			<p class="text-sm text-muted" aria-live="polite">{status}</p>
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
						<select class="field" bind:value={baudRate} disabled={connected || busy}>
							{#each ['115200', '230400', '460800', '921600', '1500000', '2000000'] as rate (rate)}
								<option value={rate}>{rate}</option>
							{/each}
						</select>
					</label>
					<label class="flex flex-col gap-2">
						<span class="eyebrow">Activation</span>
						<select class="field" bind:value={activationMs} disabled={busy}>
							{#each [0, 100, 1000, 2000, 3000, 4000, 8000, 16000] as ms (ms)}
								<option value={String(ms)}>{ms < 1000 ? `${ms} ms` : `${ms / 1000} s`}</option>
							{/each}
						</select>
					</label>
				</div>
				<div class="flex flex-wrap gap-3">
					<button
						class="btn btn-outline"
						disabled={!connected || busy}
						onclick={() => run(() => flasher.eraseAllFlash(activation()))}>Erase all flash</button
					>
					<button
						class="btn btn-outline"
						disabled={!connected || busy}
						onclick={() => run(() => flasher.softResetWithActivation(activation()))}
						>Reset chip</button
					>
				</div>
			</div>
		</details>
	{/if}
</section>
