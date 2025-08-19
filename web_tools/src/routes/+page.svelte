<script lang="ts">
	import { bleConnectionStore } from '../stores/connectionStore.svelte';
	import { logStore } from '../stores/logStore.svelte';

	// Helpers to build hex payloads
	const hb = (n: number) => n.toString(16).padStart(2, '0');

	let charByteHex = 'ff';

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
		<div class="connection bg-base-300 rounded-lg p-5 w-full prose max-w-none">
			<h3>Connection</h3>
			<button class="btn btn-primary" on:click={() => bleConnectionStore.preConnect()}>
				Connect
			</button>
			<button class="btn btn-secondary" on:click={() => bleConnectionStore.disconnect()}>
				Disconnect
			</button>
		</div>

		<div class="bg-base-300 rounded-lg p-3 prose max-w-none">
			<h3>Temperature & time</h3>
			<div class="flex flex-wrap gap-2">
				<button class="btn" on:click={setTimeNow}>Set time now</button>
				<button class="btn btn-primary" on:click={() => bleConnectionStore.sendRxTxCommand('e2aa')}>
					Get temperature
				</button>
			</div>
		</div>

		<div class="card bg-base-300 rounded-lg p-5 prose max-w-none">
			<h3>EPD control</h3>
			<div class="flex flex-wrap items-end gap-2">
				<button class="btn btn-sm" on:click={() => bleConnectionStore.sendRxTxCommand('e200')}>
					Flush (partial)
				</button>
				<div class="form-control">
					<span class="label-text">Scene</span>
					<div class="join">
						<button
							class="btn btn-sm join-item"
							on:click={() => bleConnectionStore.sendRxTxCommand('e1' + hb(1))}>1: Default</button
						>
						<button
							class="btn btn-sm join-item"
							on:click={() => bleConnectionStore.sendRxTxCommand('e1' + hb(2))}>2: Time+Date</button
						>
					</div>
				</div>

				<div class="form-control p-5">
					<span class="label-text">Fill byte (hex)</span>
					<div class="join">
						<input class="input input-sm input-bordered join-item w-24" bind:value={charByteHex} />
						<button
							class="btn btn-sm join-item"
							on:click={() => bleConnectionStore.sendRxTxCommand('b1' + (charByteHex || '00'))}
						>
							Draw
						</button>
					</div>
				</div>
			</div>
		</div>
	</div>
	<div class="mockup-code w-full lg:w-1/3 text-lime-400 text-xs shrink-0 min-h-90">
		{#each logStore.logs as log}
			<pre><code>{log}</code></pre>
		{/each}
	</div>
</div>
