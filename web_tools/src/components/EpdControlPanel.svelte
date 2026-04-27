<script lang="ts">
	import { bleConnectionStore, DISPLAY_MODEL_OPTIONS } from '../stores/connectionStore.svelte';

	let charByteHex = $state('ff');

	const hb = (n: number) => n.toString(16).padStart(2, '0');

	function setTimeNow() {
		const now = new Date();
		const unix = Math.floor(now.getTime() / 1000);
		const y = now.getFullYear();
		const m = now.getMonth() + 1;
		const d = now.getDate();
		const wd = now.getDay();
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

	function handleDisplayModelChange(event: Event) {
		const select = event.currentTarget;
		if (!(select instanceof HTMLSelectElement)) return;
		bleConnectionStore.setDisplayModel(Number(select.value));
	}

	function handleFastRefreshChange(event: Event) {
		const input = event.currentTarget;
		if (!(input instanceof HTMLInputElement)) return;
		bleConnectionStore.setFastRefreshEnabled(input.checked);
	}
</script>

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
					onchange={handleDisplayModelChange}
					disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
				>
					{#each DISPLAY_MODEL_OPTIONS as option (option.model)}
						<option value={option.model}>{option.name} ({option.width}x{option.height})</option>
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
			<div class="flex flex-wrap gap-3">
				<h4 class="w-full">LED flashing</h4>
				<button
					class="btn btn-error"
					onclick={() => bleConnectionStore.sendRxTxCommand('e300')}
					disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
					>Disable</button
				>
				<button
					class="btn btn-success"
					onclick={() => bleConnectionStore.sendRxTxCommand('e301')}
					disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
					>Enable</button
				>
			</div>
			<div class="flex flex-wrap gap-3">
				<h4 class="w-full">LED rainbow</h4>
				<button
					class="btn btn-secondary"
					onclick={() => bleConnectionStore.sendRxTxCommand('e401')}
					disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
					>Start rainbow</button
				>
				<button
					class="btn"
					onclick={() => bleConnectionStore.sendRxTxCommand('e400')}
					disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
					>Stop rainbow</button
				>
			</div>
			<div class="flex flex-wrap gap-3">
				<h4 class="w-full">Screen refresh</h4>
				<label class="cursor-pointer inline-flex items-center gap-2">
					<input
						type="checkbox"
						class="toggle toggle-primary"
						checked={bleConnectionStore.fastRefreshEnabled}
						onchange={handleFastRefreshChange}
						disabled={!bleConnectionStore.connected ||
							bleConnectionStore.isFlashingFirmware ||
							!bleConnectionStore.fastRefreshSupported}
					/>
					<span class="label-text">
						Fast refresh
						{#if !bleConnectionStore.fastRefreshSupported}
							(unsupported)
						{/if}
					</span>
				</label>
				<button
					class="btn btn-primary"
					onclick={() => bleConnectionStore.sendRxTxCommand('e200')}
					disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
					>Flush (partial)</button
				>
			</div>
			<div class="flex flex-wrap gap-3">
				<h4 class="w-full">Scene</h4>
				<button
					class="btn btn-primary"
					onclick={() => bleConnectionStore.sendRxTxCommand('e1' + hb(0))}
					disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
					>0: Image mode (no scene)</button
				>
				<button
					class="btn btn-secondary"
					onclick={() => bleConnectionStore.sendRxTxCommand('e1' + hb(1))}
					disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
					>1: Default</button
				>
				<button
					class="btn btn-accent"
					onclick={() => bleConnectionStore.sendRxTxCommand('e1' + hb(2))}
					disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
					>2: Time + Date</button
				>
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
						onclick={() => {
							const val = (charByteHex || '00').slice(0, 2);
							if (/^[0-9a-fA-F]{2}$/.test(val)) {
								bleConnectionStore.sendRxTxCommand('b1' + val);
							}
						}}
						disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
						>Draw</button
					>
				</div>
			</div>
		</div>
	</div>
</div>
