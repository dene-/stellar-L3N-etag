<script lang="ts">
	import { bleConnectionStore } from '../stores/connectionStore.svelte';
	import { logStore } from '../stores/logStore.svelte';

	let firmwareData: Uint8Array | null = $state(null);
	let firmwareFileName = $state('');
	let firmwareSize = $state(0);

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

			// Check Telink magic bytes at offset 8..11 (4b4e4c54 = "KNLT")
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

			logStore.addLog(`[${fileName}] selected, size: ${data.length} bytes`);
		};

		reader.readAsArrayBuffer(file);
	}

	function resetFileInputValue(event: Event) {
		const input = event.currentTarget;
		if (!(input instanceof HTMLInputElement)) return;
		input.value = '';
	}
</script>

<div class="collapse collapse-arrow bg-base-300 rounded-lg">
	<input type="radio" name="accordion" />
	<div class="collapse-title text-lg font-semibold">BLE firmware flash</div>
	<div class="collapse-content">
		<div class="flex flex-col gap-3">
			<div class="flex flex-col lg:flex-row flex-wrap gap-3">
				<fieldset class="fieldset p-0">
					<legend class="fieldset-legend">Select firmware</legend>
					<input
						class="file-input file-input-primary file-input-bordered"
						type="file"
						accept=".bin"
						onchange={handleFirmwareFile}
						disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
						onclick={resetFileInputValue}
					/>
				</fieldset>
			</div>
			<div class="flex flex-col gap-3">
				{#if firmwareData}
					<div class="text-sm text-base-content/70">
						{firmwareFileName} — {firmwareSize.toLocaleString()} bytes
					</div>
				{/if}
				<div class="flex gap-3 items-center">
					<progress
						class="progress progress-primary w-full h-6 rounded-lg"
						value={bleConnectionStore.firmwareUploadProgress}
						max="100"
					></progress>
					<div>{Math.ceil(bleConnectionStore.firmwareUploadProgress)}%</div>
				</div>
				<button
					disabled={!bleConnectionStore.connected ||
						bleConnectionStore.isFlashingFirmware ||
						!firmwareData}
					class="btn btn-primary self-start"
					onclick={() => {
						if (firmwareData) bleConnectionStore.flashFirmware(0x20000, firmwareData);
					}}
				>
					Upload Firmware
				</button>
			</div>
		</div>
	</div>
</div>
