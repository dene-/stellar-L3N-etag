<script lang="ts">
	import { fly, fade } from 'svelte/transition';
	import { logStore } from '../stores/logStore.svelte';
	import Icon from './ui/Icon.svelte';

	let { onclose }: { onclose: () => void } = $props();
</script>

<svelte:window onkeydown={(event) => event.key === 'Escape' && onclose()} />

<div
	class="fixed inset-0 z-40 bg-ink/60"
	transition:fade={{ duration: 150 }}
	onclick={onclose}
	aria-hidden="true"
></div>
<aside
	class="fixed inset-y-0 right-0 z-50 flex w-full max-w-lg flex-col border-l border-line bg-base"
	transition:fly={{ x: 40, duration: 200 }}
	aria-label="Activity log"
>
	<div class="flex h-20 items-center gap-4 border-b border-line px-6">
		<h2 class="eyebrow text-fg!">Activity</h2>
		<button class="btn btn-text ml-auto" onclick={() => logStore.clearLogs()}>Clear</button>
		<button class="icon-btn" aria-label="Close" onclick={onclose} {@attach (node) => node.focus()}>
			<Icon name="close" size={18} />
		</button>
	</div>
	<ol class="flex-1 overflow-auto px-6 py-4 font-mono text-xs leading-6 text-soft">
		{#each logStore.logs as log, index (`${logStore.total - index}`)}
			<li class="border-b border-line/50 py-1 break-words">{log}</li>
		{:else}
			<li class="text-muted">
				Nothing yet. Connecting, uploads and replies from the tag show up here.
			</li>
		{/each}
	</ol>
</aside>
