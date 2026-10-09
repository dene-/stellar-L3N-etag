<script lang="ts">
	import AppHeader, { type Tab } from '../components/AppHeader.svelte';
	import LogDrawer from '../components/LogDrawer.svelte';
	import StatusBar from '../components/StatusBar.svelte';
	import DeviceView from '../components/device/DeviceView.svelte';
	import ImagesView from '../components/images/ImagesView.svelte';
	import FirmwareView from '../components/firmware/FirmwareView.svelte';
	import { logStore } from '../stores/logStore.svelte';
	import { transfers } from '../stores/transferStore.svelte';

	const TABS: Tab[] = ['device', 'images', 'firmware'];

	function tabFromHash(): Tab {
		const hash = location.hash.slice(1) as Tab;
		return TABS.includes(hash) ? hash : 'device';
	}

	let tab = $state(tabFromHash());
	let logOpen = $state(false);
	let seenLogs = $state(0);

	function toggleLog() {
		logOpen = !logOpen;
		seenLogs = logStore.total;
	}

	// Closing or reloading the page cuts a transfer off; the browser asks first.
	function confirmLeave(event: BeforeUnloadEvent) {
		if (transfers.active) event.preventDefault();
	}
</script>

<svelte:window onhashchange={() => (tab = tabFromHash())} onbeforeunload={confirmLeave} />

<div class="mx-auto flex min-h-dvh max-w-[1440px] flex-col">
	<AppHeader {tab} {logOpen} {seenLogs} onlog={toggleLog} />

	<main class="mx-auto w-full max-w-7xl flex-1 px-5 py-10 sm:px-10 lg:py-16">
		{#if tab === 'device'}
			<DeviceView />
		{:else if tab === 'images'}
			<ImagesView />
		{:else}
			<FirmwareView />
		{/if}
	</main>

	<StatusBar onlog={toggleLog} />
</div>

{#if logOpen}
	<LogDrawer
		onclose={() => {
			logOpen = false;
			seenLogs = logStore.total;
		}}
	/>
{/if}
