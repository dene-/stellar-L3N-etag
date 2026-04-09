<script lang="ts">
	import {
		bleConnectionStore,
		DISPLAY_MODEL_OPTIONS
	} from '../stores/connectionStore.svelte';
	import { logStore } from '../stores/logStore.svelte';
	import { bwPalette, bwrPalette, ditheringCanvasByPalette, bytesToHex } from '$lib/utils';
	import pica from 'pica';

	// Helpers to build hex payloads
	const hb = (n: number) => n.toString(16).padStart(2, '0');

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

	function resizeCanvasToDisplay(targetWidth: number, targetHeight: number) {
		if (!canvasEl) return;

		if (canvasEl.width === targetWidth && canvasEl.height === targetHeight) return;

		const previousImageData = originalImageData;
		canvasEl.width = targetWidth;
		canvasEl.height = targetHeight;

		const ctx = canvasEl.getContext('2d', { willReadFrequently: true });
		if (!ctx) return;

		ctx.fillStyle = '#fff';
		ctx.fillRect(0, 0, targetWidth, targetHeight);

		if (!previousImageData) return;

		const sourceCanvas = document.createElement('canvas');
		sourceCanvas.width = previousImageData.width;
		sourceCanvas.height = previousImageData.height;

		const sourceCtx = sourceCanvas.getContext('2d', { willReadFrequently: true });
		if (!sourceCtx) return;

		sourceCtx.putImageData(previousImageData, 0, 0);
		ctx.drawImage(sourceCanvas, 0, 0, previousImageData.width, previousImageData.height, 0, 0, targetWidth, targetHeight);
		originalImageData = ctx.getImageData(0, 0, targetWidth, targetHeight);
		applyDithering();
	}

	$effect(() => {
		resizeCanvasToDisplay(bleConnectionStore.displayWidth, bleConnectionStore.displayHeight);
	});

	function handleImageFile(event: Event) {
		const input = event.target as HTMLInputElement;

		if (!canvasEl || !input.files || input.files.length === 0) return;

		const file = input.files[0];

		const img = new Image();
		img.src = URL.createObjectURL(file);

		img.onload = async () => {
			try {
				const ctx = canvasEl!.getContext('2d', { willReadFrequently: true });

				if (!ctx) return;

				const canvasCopy = canvasEl?.cloneNode(true) as HTMLCanvasElement;

				const resizedPhoto = await pica().resize(img, canvasCopy!);

				ctx.clearRect(0, 0, canvasEl!.width, canvasEl!.height);
				ctx.drawImage(
					resizedPhoto,
					0,
					0,
					resizedPhoto.width,
					resizedPhoto.height,
					0,
					0,
					canvasEl!.width,
					canvasEl!.height
				);

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

<div class="flex flex-col flex-grow lg:flex-row gap-3 pb-5">
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
			<div class="mt-3 text-sm opacity-80">
				<div>Device: {bleConnectionStore.connectedDeviceName || 'Not connected'}</div>
				<div>
					Display: {bleConnectionStore.deviceModelName} ({bleConnectionStore.displayWidth}x{bleConnectionStore.displayHeight})
					via {bleConnectionStore.displaySource}
				</div>
			</div>
		</div>

		<div class="collapse collapse-arrow bg-base-300 rounded-lg">
			<input type="radio" name="accordion" checked={true} />
			<div class="collapse-title text-lg font-semibold">Temperature & time</div>
			<div class="collapse-content">
				<div class="prose max-w-none p-0">
					<div class="flex gap-3 flex-wrap">
						<button
							class="btn btn-accent"
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
			<input type="radio" name="accordion" />
			<div class="collapse-title text-lg font-semibold">EPD control</div>
			<div class="collapse-content">
				<div class="prose max-w-none p-0 flex items-start flex-col gap-3">
					<div class="flex flex-wrap gap-3">
						<h4 class="w-full">Display model</h4>
						<select
							class="select select-bordered select-primary w-60"
							value={String(bleConnectionStore.deviceModel)}
							onchange={(event) =>
								bleConnectionStore.setDisplayModel(
									Number((event.currentTarget as HTMLSelectElement).value)
								)}
							disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
						>
							{#each DISPLAY_MODEL_OPTIONS as option (option.model)}
								<option value={option.model}>
									{option.name} ({option.width}x{option.height})
								</option>
							{/each}
						</select>
						<button
							class="btn btn-secondary"
							onclick={() => bleConnectionStore.queryDisplayInfo()}
							disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
						>
							Query device
						</button>
					</div>
					<!-- LED flashing control (moved here) -->
					<div class="flex flex-wrap gap-3">
						<h4 class="w-full">LED flashing</h4>
						<button
							class="btn btn-error"
							onclick={() => bleConnectionStore.sendRxTxCommand('e300')}
							disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
						>
							Disable
						</button>
						<button
							class="btn btn-success"
							onclick={() => bleConnectionStore.sendRxTxCommand('e301')}
							disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
						>
							Enable
						</button>
					</div>
					<div class="flex flex-wrap gap-3">
						<h4 class="w-full">LED rainbow</h4>
						<button
							class="btn btn-secondary"
							onclick={() => bleConnectionStore.sendRxTxCommand('e401')}
							disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
						>
							Start rainbow
						</button>
						<button
							class="btn"
							onclick={() => bleConnectionStore.sendRxTxCommand('e400')}
							disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
						>
							Stop rainbow
						</button>
					</div>
					<div class="flex flex-wrap gap-3">
						<h4 class="w-full">Screen refresh</h4>
						<button
							class="btn btn-primary"
							onclick={() => bleConnectionStore.sendRxTxCommand('e200')}
							disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
						>
							Flush (partial)
						</button>
					</div>
					<div class="flex flex-wrap gap-3">
						<h4 class="w-full">Scene</h4>
						<button
							class="btn btn-primary"
							onclick={() => bleConnectionStore.sendRxTxCommand('e1' + hb(0))}
							disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
						>
							0: Image mode (no scene)
						</button>
						<button
							class="btn btn-secondary"
							onclick={() => bleConnectionStore.sendRxTxCommand('e1' + hb(1))}
							disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
						>
							1: Default
						</button>
						<button
							class="btn btn-accent"
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
								class="input input-primary input-bordered join-item w-24"
								bind:value={charByteHex}
								disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
							/>
							<button
								class="btn btn-primary join-item"
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
								accept=".png,.jpg,.jpeg,.bmp,.webp"
								onchange={handleImageFile}
								disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
							/>
						</fieldset>

						<fieldset class="fieldset p-0">
							<legend class="fieldset-legend">Dithering</legend>
							<select
								bind:value={ditheringMode}
								class="select select-bordered select-primary w-60"
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

						<fieldset class="fieldset p-0">
							<legend class="fieldset-legend">Serpentine</legend>
							<label class="cursor-pointer inline-flex items-center gap-2">
								<input
									type="checkbox"
									class="checkbox checkbox-primary"
									bind:checked={serpentine}
									disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
									onchange={applyDithering}
								/>
								<span class="label-text">Enable</span>
							</label>
						</fieldset>
					</div>

					<div class="flex flex-col gap-3">
						<div class="self-start bg-base-100 rounded-lg p-3 w-full max-w-[500px]">
							<div class="mb-2 text-sm text-base-content/70">
								Canvas: {bleConnectionStore.displayWidth}x{bleConnectionStore.displayHeight}
							</div>
							<canvas
								use:onCanvasReady
								width={bleConnectionStore.displayWidth}
								height={bleConnectionStore.displayHeight}
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

		<!-- Firmware flashing -->
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
								onclick={(event: Event) => {
									if (event.target) {
										(event.target as HTMLInputElement).value = '';
									}
								}}
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

		<!-- <div class="collapse collapse-arrow bg-base-300 rounded-lg">
			<input type="radio" name="accordion" checked />
			<div class="collapse-title text-lg font-semibold">Weather</div>
			<div class="collapse-content">
				<div class="prose max-w-none p-0">
					<div class="flex flex-col lg:flex-row gap-3">
						<fieldset class="fieldset p-0">
							<legend class="fieldset-legend">Location</legend>
							<input
								class="input input-primary"
								type="text"
								bind:value={location}
								disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
							/>
						</fieldset>
						<fieldset class="fieldset p-0">
							<legend class="fieldset-legend hidden lg:opacity-0">a</legend>
							<button
								disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
								class="btn btn-primary"
								onclick={() => weatherStore.getForecast(location)}
							>
								Get forecast
							</button>
						</fieldset>
					</div>
					<div id="capture" class="w-[250px] h-[128px] flex pb-[6px] bg-white"></div>
				</div>
			</div>
		</div> -->
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
			{#each logStore.logs as log, index (`${index}-${log}`)}
				<pre><code>{log}</code></pre>
			{/each}
		</div>
	</div>
</div>
