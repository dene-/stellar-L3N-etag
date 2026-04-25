<script lang="ts">
	import { onDestroy } from 'svelte';
	import { TlsrSerialFlasher } from '$lib/tlsr-serial-flasher';
	import { logStore } from '../stores/logStore.svelte';

	let firmwareData: Uint8Array | null = $state(null);
	let firmwareFileName = $state('');
	let firmwareSize = $state(0);
	let serialSupported = $state(TlsrSerialFlasher.isSupported());
	let serialConnected = $state(false);
	let serialBusy = $state(false);
	let serialProgress = $state(0);
	let serialStatus = $state('Open serial port and select a firmware file');
	let serialBaudRate = $state('460800');
	let serialActivationTime = $state('3000');

	const serialFlasher = new TlsrSerialFlasher({
		log: (message) => logStore.addLog(message),
		onStatus: (message) => {
			serialStatus = message;
		},
		onProgress: (percent) => {
			serialProgress = percent;
		},
		onConnectionChange: (connected) => {
			serialConnected = connected;
			if (!connected) {
				serialProgress = 0;
				serialStatus = 'Open serial port and select a firmware file';
			}
		}
	});

	onDestroy(() => {
		if (serialConnected) {
			void serialFlasher.close();
		}
	});

	function handleFirmwareFile(event: Event) {
		const input = event.target as HTMLInputElement;

		if (!input.files || !input.files[0]) {
			logStore.addLog('No file selected');
			return;
		}

		const file = input.files[0];

		if (file.size > 512 * 1024) {
			logStore.addLog('Firmware file too large (max 512KB).');
			firmwareData = null;
			return;
		}

		const reader = new FileReader();
		const fileName = file.name;

		reader.onload = function () {
			const data = new Uint8Array(this.result as ArrayBuffer);

			if (
				data.length < 12 ||
				data[8] !== 0x4b ||
				data[9] !== 0x4e ||
				data[10] !== 0x4c ||
				data[11] !== 0x54
			) {
				logStore.addLog('Selected file is not a Telink firmware .bin');
				firmwareData = null;
				firmwareFileName = '';
				firmwareSize = 0;
				return;
			}

			firmwareData = data;
			firmwareFileName = fileName;
			firmwareSize = data.length;

			serialStatus = serialConnected
				? 'Ready to flash over serial'
				: 'Open serial port to enable flashing';

			logStore.addLog(`[${fileName}] selected, size: ${data.length} bytes`);
		};

		reader.readAsArrayBuffer(file);
	}

	function resetFileInputValue(event: Event) {
		const input = event.currentTarget;
		if (!(input instanceof HTMLInputElement)) return;
		input.value = '';
	}

	function serialActionDisabled() {
		return !serialSupported || !serialConnected || serialBusy;
	}

	async function toggleSerialConnection() {
		if (!serialSupported || serialBusy) {
			return;
		}

		try {
			if (serialConnected) {
				await serialFlasher.close();
				return;
			}

			await serialFlasher.open(Number(serialBaudRate));

			serialStatus = firmwareData ? 'Ready to flash over serial' : 'Select a firmware file';
		} catch (error) {
			console.error(error);

			logStore.addLog(
				'Serial open error: ' + (error instanceof Error ? error.message : String(error))
			);

			serialStatus = 'Failed to open serial port';
		}
	}

	async function runSerialAction(action: () => Promise<void>) {
		serialBusy = true;

		try {
			await action();
		} catch (error) {
			console.error(error);

			logStore.addLog(
				'Serial flashing error: ' + (error instanceof Error ? error.message : String(error))
			);

			serialStatus = 'Serial action failed';
		} finally {
			serialBusy = false;
		}
	}

	async function flashOverSerial() {
		if (!firmwareData) {
			return;
		}

		await runSerialAction(() =>
			serialFlasher.flashFirmware(firmwareData!, Number(serialActivationTime))
		);
	}
</script>

<div class="collapse collapse-arrow bg-base-300 rounded-lg">
	<input type="radio" name="accordion" />
	<div class="collapse-title text-lg font-semibold">Serial firmware flash</div>
	<div class="collapse-content">
		<div class="flex flex-col gap-4">
			<div class="flex flex-col lg:flex-row flex-wrap gap-3">
				<fieldset class="fieldset p-0">
					<legend class="fieldset-legend">USB-COM</legend>
					<button
						type="button"
						class="btn btn-primary"
						onclick={toggleSerialConnection}
						disabled={!serialSupported || serialBusy}
					>
						{serialConnected ? 'Close' : 'Open'}
					</button>
				</fieldset>

				<fieldset class="fieldset p-0">
					<legend class="fieldset-legend">Baud</legend>
					<select
						class="select select-bordered select-primary"
						bind:value={serialBaudRate}
						disabled={serialConnected || serialBusy}
					>
						<option value="115200">115200</option>
						<option value="230400">230400</option>
						<option value="460800">460800</option>
						<option value="921600">921600</option>
						<option value="1500000">1500000</option>
						<option value="2000000">2000000</option>
					</select>
				</fieldset>

				<fieldset class="fieldset p-0">
					<legend class="fieldset-legend">Activation</legend>
					<select
						class="select select-bordered select-primary"
						bind:value={serialActivationTime}
						disabled={serialBusy}
					>
						<option value="0">0 ms</option>
						<option value="100">100 ms</option>
						<option value="1000">1 sec</option>
						<option value="2000">2 sec</option>
						<option value="3000">3 sec</option>
						<option value="4000">4 sec</option>
						<option value="8000">8 sec</option>
						<option value="16000">16 sec</option>
					</select>
				</fieldset>

				<fieldset class="fieldset p-0">
					<legend class="fieldset-legend">Select firmware</legend>
					<input
						class="file-input file-input-primary file-input-bordered"
						type="file"
						accept=".bin"
						onchange={handleFirmwareFile}
						disabled={serialBusy}
						onclick={resetFileInputValue}
					/>
				</fieldset>
			</div>

			{#if firmwareData}
				<div class="text-sm text-base-content/70">
					{firmwareFileName} — {firmwareSize.toLocaleString()} bytes
				</div>
			{/if}

			<div class="flex flex-col gap-3 text-sm text-base-content/70">
				<div>
					Uses Web Serial with DTR/RTS toggling. Connect TX-SWS as in the UART flashing pinout.
				</div>

				<img
					src="https://raw.githubusercontent.com/atc1441/ATC_TLSR_Paper/main/USB_UART_Flashing_connection.jpg"
					alt="UART flashing pinout"
					class="w-full max-w-xl rounded-lg border border-base-300 bg-base-100"
					loading="lazy"
				/>
			</div>

			<div class="flex gap-3 items-center pt-1">
				<progress
					class="progress progress-primary w-full h-6 rounded-lg"
					value={serialProgress}
					max="100"
				></progress>
				<div>{Math.ceil(serialProgress)}%</div>
			</div>

			<div class="text-sm text-base-content/70">{serialStatus}</div>

			<div class="flex flex-wrap gap-3">
				<button
					disabled={serialActionDisabled() || !firmwareData}
					class="btn btn-primary"
					onclick={flashOverSerial}
				>
					Write to Flash
				</button>
				<button
					disabled={serialActionDisabled()}
					class="btn btn-outline"
					onclick={() =>
						runSerialAction(() => serialFlasher.unlockFlash(Number(serialActivationTime)))}
				>
					Unlock Flash
				</button>
				<button
					disabled={serialActionDisabled()}
					class="btn btn-outline"
					onclick={() =>
						runSerialAction(() => serialFlasher.eraseAllFlash(Number(serialActivationTime)))}
				>
					Erase All Flash
				</button>
				<button
					disabled={serialActionDisabled()}
					class="btn btn-outline"
					onclick={() =>
						runSerialAction(() =>
							serialFlasher.softResetWithActivation(Number(serialActivationTime))
						)}
				>
					Soft Reset MCU
				</button>
			</div>

			{#if !serialSupported}
				<div class="text-sm text-error">This browser does not expose Web Serial.</div>
			{/if}
		</div>
	</div>
</div>
