<script lang="ts">
	import { logStore } from '../stores/logStore.svelte';
</script>

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
