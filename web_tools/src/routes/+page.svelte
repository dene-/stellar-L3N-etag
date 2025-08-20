<script lang="ts">
	import { bleConnectionStore } from '../stores/connectionStore.svelte';
	import { logStore } from '../stores/logStore.svelte';
	import { bwPalette, bwrPalette, ditheringCanvasByPalette, bytesToHex } from '$lib/utils';

	// Helpers to build hex payloads
	const hb = (n: number) => n.toString(16).padStart(2, '0');

	let openTemp = $state(true);
	let openEpd = $state(true);
	let openImage = $state(true);
	let openFirmware = $state(true);

	let charByteHex = $state('ff');

	let ditheringMode: string = $state('bwr_Atkinson');
	let serpentine = $state(false);
	let canvasEl: HTMLCanvasElement | null = null;
	let originalImageData: ImageData | null = null;

	let firmwareArray = $state('');

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

	function handleImageFile(event: Event) {
		const input = event.target as HTMLInputElement;

		if (!canvasEl || !input.files || input.files.length === 0) return;

		const file = input.files[0];

		const img = new Image();
		img.src = URL.createObjectURL(file);

		img.onload = () => {
			try {
				const ctx = canvasEl!.getContext('2d', { willReadFrequently: true });

				if (!ctx) return;

				ctx.clearRect(0, 0, canvasEl!.width, canvasEl!.height);
				ctx.drawImage(img, 0, 0, img.width, img.height, 0, 0, canvasEl!.width, canvasEl!.height);

				originalImageData = ctx.getImageData(0, 0, canvasEl!.width, canvasEl!.height);

				applyDithering();
			} finally {
				URL.revokeObjectURL(img.src);
			}
		};
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

	function applyDithering() {
		if (!canvasEl) return;
		const ctx = canvasEl.getContext('2d', { willReadFrequently: true });
		if (!ctx) return;
		if (originalImageData) ctx.putImageData(originalImageData, 0, 0);
		const isBwr = ditheringMode.startsWith('bwr_');
		const kern = ditheringMode.split('_')[1] || 'Atkinson';
		ditheringCanvasByPalette(canvasEl, isBwr ? bwrPalette : bwPalette, kern, {
			dithSerp: serpentine
		});
	}

	async function uploadImage() {
		if (!canvasEl) return;
		await bleConnectionStore.uploadImageFromCanvas(canvasEl);
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

<div class="flex flex-col lg:flex-row gap-3">
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
		</div>

		<div class="collapse collapse-arrow bg-base-300 rounded-lg">
			<input type="checkbox" bind:checked={openTemp} />
			<div class="collapse-title text-lg font-semibold">Temperature & time</div>
			<div class="collapse-content">
				<div class="prose max-w-none p-0">
					<div class="flex gap-3">
						<button
							class="btn"
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
			<input type="checkbox" bind:checked={openEpd} />
			<div class="collapse-title text-lg font-semibold">EPD control</div>
			<div class="collapse-content">
				<div class="prose max-w-none p-0 flex items-start flex-col gap-3">
					<button
						class="btn"
						onclick={() => bleConnectionStore.sendRxTxCommand('e200')}
						disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
					>
						Flush (partial)
					</button>
					<div class="flex flex-wrap gap-3">
						<h4 class="w-full">Scene</h4>
						<button
							class="btn"
							onclick={() => bleConnectionStore.sendRxTxCommand('e1' + hb(0))}
							disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
						>
							0: Image mode (no scene)
						</button>
						<button
							class="btn"
							onclick={() => bleConnectionStore.sendRxTxCommand('e1' + hb(1))}
							disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
						>
							1: Default
						</button>
						<button
							class="btn"
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
								class="input input-sm input-bordered join-item w-24"
								bind:value={charByteHex}
								disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
							/>
							<button
								class="btn btn-sm join-item"
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
			<input type="checkbox" bind:checked={openImage} />
			<div class="collapse-title text-lg font-semibold">Image upload</div>
			<div class="collapse-content">
				<div class="flex flex-col gap-3">
					<div class="flex flex-col lg:flex-row flex-wrap gap-3">
						<fieldset class="fieldset">
							<legend class="fieldset-legend">Choose image</legend>
							<input
								class="file-input file-input-bordered file-input-sm"
								type="file"
								accept=".png,.jpg,.jpeg,.bmp,.webp"
								onchange={handleImageFile}
								disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
							/>
						</fieldset>

						<fieldset class="fieldset">
							<legend class="fieldset-legend">Dithering</legend>
							<select
								bind:value={ditheringMode}
								class="select select-bordered select-sm w-60"
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

						<fieldset class="fieldset">
							<legend class="fieldset-legend">Serpentine</legend>
							<label class="cursor-pointer inline-flex items-center gap-2">
								<input
									type="checkbox"
									class="checkbox checkbox-sm"
									bind:checked={serpentine}
									disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
									onchange={applyDithering}
								/>
								<span class="label-text">Enable</span>
							</label>
						</fieldset>
					</div>

					<div class="flex flex-col gap-2">
						<div class="self-start bg-base-100 rounded-lg p-3 w-full max-w-[500px]">
							<canvas
								use:onCanvasReady
								width="250"
								height="128"
								class="border w-full"
								style="image-rendering: pixelated"
							></canvas>
						</div>
						<button
							disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
							class="btn btn-primary self-start"
							onclick={uploadImage}
						>
							Upload to Display Now
						</button>
					</div>
				</div>
			</div>
		</div>

		<div class="collapse collapse-arrow bg-base-300 rounded-lg">
			<input type="checkbox" bind:checked={openFirmware} />
			<div class="collapse-title text-lg font-semibold">Flash firmware</div>
			<div class="collapse-content">
				<div class="flex flex-col gap-3">
					<div class="flex flex-col lg:flex-row flex-wrap gap-3">
						<fieldset class="fieldset">
							<legend class="fieldset-legend">Select firmware</legend>
							<input
								class="file-input file-input-bordered file-input-sm"
								type="file"
								accept=".bin"
								onchange={handleFirmwareFile}
								disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
								onclick={(event: Event) => {
									event.target && ((event.target as HTMLInputElement).value = '');
								}}
							/>
						</fieldset>
					</div>

					<div class="flex flex-col gap-3">
						<textarea
							name="firmware"
							id="firmware"
							class="textarea w-full resize-none max-w-[500px] focus:outline-none active:outline-none select-none pointer-events-none"
							bind:value={firmwareArray}
						></textarea>
						<progress
							class="progress progress-primary w-full max-w-[500px]"
							value={bleConnectionStore.firmwareUploadProgress}
							max="100"
						></progress>
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
			{#each logStore.logs as log}
				<pre><code>{log}</code></pre>
			{/each}
		</div>
	</div>
</div>
