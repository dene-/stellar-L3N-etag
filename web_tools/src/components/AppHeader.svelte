<script module lang="ts">
	export type Tab = 'device' | 'images' | 'firmware';
</script>

<script lang="ts">
	import { bleConnectionStore } from '../stores/connectionStore.svelte';
	import { logStore } from '../stores/logStore.svelte';
	import Icon from './ui/Icon.svelte';
	import Logo from './ui/Logo.svelte';

	type Props = { tab: Tab; logOpen: boolean; seenLogs: number; onlog: () => void };
	let { tab, logOpen, seenLogs, onlog }: Props = $props();

	const tabs: { id: Tab; label: string }[] = [
		{ id: 'device', label: 'Device' },
		{ id: 'images', label: 'Images' },
		{ id: 'firmware', label: 'Firmware' }
	];

	let unseen = $derived(!logOpen && logStore.total > seenLogs);
</script>

{#snippet navLinks(className: string)}
	{#each tabs as item (item.id)}
		<a
			href="#{item.id}"
			class="{className} relative inline-flex min-h-11 items-center justify-center text-[0.8rem] font-medium tracking-[0.16em] uppercase transition-colors after:absolute after:bottom-0 after:left-1/2 after:h-0.5 after:w-4 after:-translate-x-1/2 after:bg-accent after:opacity-0 after:transition-opacity aria-[current=page]:text-fg aria-[current=page]:after:opacity-100 {tab ===
			item.id
				? ''
				: 'text-muted hover:text-soft'}"
			aria-current={tab === item.id ? 'page' : undefined}>{item.label}</a
		>
	{/each}
{/snippet}

<header class="border-b border-line">
	<div class="mx-auto flex h-20 max-w-7xl items-center gap-6 px-5 sm:px-10">
		<a href="#device" class="flex items-center gap-3 text-fg" aria-label="Stellar tools, device">
			<Logo />
			<span class="eyebrow hidden text-soft! sm:inline">Stellar</span>
		</a>

		<nav class="ml-auto hidden gap-12 md:flex" aria-label="Sections">
			{@render navLinks('')}
		</nav>

		<div class="ml-auto flex items-center gap-4 md:ml-10">
			{#if bleConnectionStore.connected}
				<span class="hidden items-center gap-2 text-sm text-soft sm:flex">
					<span class="size-2 rounded-full bg-ok" aria-hidden="true"></span>
					{bleConnectionStore.connectedDeviceName}
				</span>
				<button
					class="btn btn-text"
					disabled={bleConnectionStore.busy}
					onclick={() => bleConnectionStore.disconnect()}>Disconnect</button
				>
			{:else}
				<button
					class="btn btn-outline"
					disabled={bleConnectionStore.preconnected}
					onclick={() => bleConnectionStore.preConnect()}
				>
					<Icon name="bluetooth" size={16} />
					{bleConnectionStore.preconnected ? 'Connecting…' : 'Connect'}
				</button>
			{/if}
			<button
				class="relative grid size-11 place-items-center text-soft hover:text-fg"
				aria-label="Activity log"
				aria-expanded={logOpen}
				onclick={onlog}
			>
				<Icon name="menu" size={22} />
				{#if unseen}
					<span class="absolute top-2.5 right-2 size-1.5 rounded-full bg-accent" aria-hidden="true"
					></span>
				{/if}
			</button>
		</div>
	</div>

	<nav class="flex border-t border-line md:hidden" aria-label="Sections">
		{@render navLinks('flex-1 h-12')}
	</nav>
</header>
