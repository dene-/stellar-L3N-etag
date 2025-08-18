<script>
	import { bleConnectionStore } from '../stores/connectionStore.svelte';
	import { logStore } from '../stores/logStore.svelte';
</script>

<button class="btn btn-primary" on:click={() => bleConnectionStore.preConnect()}>Connect</button>
<button class="btn btn-primary" on:click={() => bleConnectionStore.sendRxTxCommand('e2aa')}>
	Get temperature
</button>
<div class="mockup-code w-full text-lime-400 text-xs">
	{#each logStore.logs as log}
		<pre><code>{log}</code></pre>
	{/each}
</div>
