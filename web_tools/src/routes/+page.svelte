<script lang="ts">
	import { bleConnectionStore } from '../stores/connectionStore.svelte';
	import { logStore } from '../stores/logStore.svelte';
	import { bwPalette, bwrPalette, ditheringCanvasByPalette } from '$lib/utils';

	// Helpers to build hex payloads
	const hb = (n: number) => n.toString(16).padStart(2, '0');

	let charByteHex = $state('ff');
	let openTemp = $state(true);
	let openEpd = $state(true);

	// Image upload section state
	let openImage = $state(true);
	let ditheringMode: string = $state('bwr_Atkinson');
	let serpentine = $state(false);
	let canvasEl: HTMLCanvasElement | null = null;
	let originalImageData: ImageData | null = null;

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

	function handleImageFile(ev: Event) {
		const input = ev.target as HTMLInputElement;
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
				<button class="btn btn-primary" onclick={() => bleConnectionStore.preConnect()}>
					Connect
				</button>
				<button class="btn btn-secondary" onclick={() => bleConnectionStore.disconnect()}>
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
						<button class="btn" onclick={setTimeNow}>Set time now</button>
						<button
							class="btn btn-primary"
							onclick={() => bleConnectionStore.sendRxTxCommand('e2aa')}
							disabled={!bleConnectionStore.connected}
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
						disabled={!bleConnectionStore.connected}
					>
						Flush (partial)
					</button>
					<div class="form-control flex flex-col">
						<h4>Scene</h4>
						<div class="join">
							<button
								class="btn join-item"
								onclick={() => bleConnectionStore.sendRxTxCommand('e1' + hb(0))}
								disabled={!bleConnectionStore.connected}
							>
								0: Image mode (no scene)
							</button>
							<button
								class="btn join-item"
								onclick={() => bleConnectionStore.sendRxTxCommand('e1' + hb(1))}
								disabled={!bleConnectionStore.connected}
							>
								1: Default
							</button>
							<button
								class="btn join-item"
								onclick={() => bleConnectionStore.sendRxTxCommand('e1' + hb(2))}
								disabled={!bleConnectionStore.connected}
							>
								2: Time + Date
							</button>
						</div>
					</div>

					<div class="flex flex-col">
						<h4>Fill byte (hex)</h4>
						<div class="join">
							<input
								class="input input-sm input-bordered join-item w-24"
								bind:value={charByteHex}
								disabled={!bleConnectionStore.connected}
							/>
							<button
								class="btn btn-sm join-item"
								onclick={() => bleConnectionStore.sendRxTxCommand('b1' + (charByteHex || '00'))}
								disabled={!bleConnectionStore.connected}
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
				<div class="prose max-w-none p-0 flex flex-col gap-3">
					<div class="flex flex-wrap items-end gap-3">
						<div class="form-control">
							<label class="label"><span class="label-text">Choose image</span></label>
							<input
								class="file-input file-input-bordered file-input-sm"
								type="file"
								accept=".png,.jpg,.bmp,.webp"
								onchange={handleImageFile}
								disabled={!bleConnectionStore.connected}
							/>
						</div>
						<div class="form-control">
							<label class="label"><span class="label-text">Dithering</span></label>
							<select
								bind:value={ditheringMode}
								class="select select-bordered select-sm w-60"
								onchange={applyDithering}
								disabled={!bleConnectionStore.connected}
							>
								<optgroup label="BW">
									<option value="bw_FloydSteinberg">FloydSteinberg</option>
									<option value="bw_FalseFloydSteinberg">FalseFloydSteinberg</option>
									<option value="bw_Stucki">Stucki</option>
									<option value="bw_Atkinson">Atkinson</option>
									<option value="bw_Jarvis">Jarvis</option>
									<option value="bw_Burkes">Burkes</option>
									<option value="bw_Sierra">Sierra</option>
									<option value="bw_TwoSierra">TwoSierra</option>
									<option value="bw_SierraLite">SierraLite</option>
								</optgroup>
								<optgroup label="BWR">
									<option value="bwr_FloydSteinberg">FloydSteinberg</option>
									<option value="bwr_FalseFloydSteinberg">FalseFloydSteinberg</option>
									<option value="bwr_Stucki">Stucki</option>
									<option value="bwr_Atkinson">Atkinson</option>
									<option value="bwr_Jarvis">Jarvis</option>
									<option value="bwr_Burkes">Burkes</option>
									<option value="bwr_Sierra">Sierra</option>
									<option value="bwr_TwoSierra">TwoSierra</option>
									<option value="bwr_SierraLite">SierraLite</option>
								</optgroup>
							</select>
						</div>
						<label class="label cursor-pointer gap-2 mt-6">
							<input
								type="checkbox"
								class="checkbox checkbox-sm"
								bind:checked={serpentine}
								disabled={!bleConnectionStore.connected}
								onchange={applyDithering}
							/>
							<span class="label-text">Serpentine</span>
						</label>
					</div>

					<div class="flex flex-col gap-2">
						<div class="inline-block self-start bg-base-100 rounded-lg p-3">
							<canvas
								use:onCanvasReady
								width="250"
								height="128"
								class="eink border rounded"
								style="image-rendering: pixelated; width: 500px; height: 256px;"
							></canvas>
						</div>
						<button
							disabled={!bleConnectionStore.connected}
							class="btn btn-primary self-start"
							onclick={uploadImage}
						>
							Upload to Display Now
						</button>
					</div>
				</div>
			</div>
		</div>
	</div>
	<div
		class="rounded-lg bg-base-300 p-3 w-full lg:w-1/3 max-h-[70vh] flex flex-col overflow-auto shrink-0 gap-3"
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
