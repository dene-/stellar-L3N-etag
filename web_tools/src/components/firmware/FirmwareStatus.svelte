<script lang="ts">
	import { onMount } from 'svelte';
	import { bleConnectionStore } from '../../stores/connectionStore.svelte';
	import { releasesStore } from '../../stores/releasesStore.svelte';

	// The connected tag's firmware, with a heads-up when a newer release exists. link: point the
	// heads-up at the Firmware page.
	let { link = false }: { link?: boolean } = $props();

	const store = bleConnectionStore;
	let installed = $derived(
		store.firmwareVersion ?? (store.firmwareVersionQueried ? 'before 0.10.0' : '…')
	);
	let update = $derived(
		store.firmwareVersionQueried ? releasesStore.updateFor(store.firmwareVersion) : null
	);

	onMount(() => void releasesStore.load());
</script>

<p class="flex flex-wrap items-center gap-x-3 gap-y-1 text-sm text-soft">
	<span>Firmware {installed}</span>
	{#if update}
		<span class="flex items-center gap-2 text-accent">
			<span class="size-1.5 rounded-full bg-accent" aria-hidden="true"></span>
			{#if link}
				<a class="underline-offset-4 hover:underline" href="#firmware"
					>{update.version} available, update</a
				>
			{:else}
				{update.version} available
			{/if}
		</span>
	{:else if store.firmwareVersionQueried && releasesStore.latest && store.firmwareVersion}
		<span class="text-muted">· up to date</span>
	{/if}
</p>
