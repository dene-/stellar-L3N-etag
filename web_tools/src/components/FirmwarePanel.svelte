<script lang="ts">
	import { bleConnectionStore } from '../stores/connectionStore.svelte';
	import { logStore } from '../stores/logStore.svelte';
	import { bytesToHex } from '$lib/utils';

	let firmwareArray = $state('');

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

	function resetFileInputValue(event: Event) {
		const input = event.currentTarget;
		if (!(input instanceof HTMLInputElement)) return;
		input.value = '';
	}
</script>

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
