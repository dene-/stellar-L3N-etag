<script lang="ts">
	import { bleConnectionStore } from '../../stores/connectionStore.svelte';
	import FirmwareStatus from '../firmware/FirmwareStatus.svelte';
	import DeviceSettings from './DeviceSettings.svelte';
	import SceneCards from './SceneCards.svelte';

	const bluetoothAvailable = typeof navigator !== 'undefined' && 'bluetooth' in navigator;
	const store = bleConnectionStore;

	function formatTime(date: Date | null) {
		return date ? date.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' }) : '—';
	}
</script>

<div class="grid items-start gap-14 lg:grid-cols-[minmax(0,5fr)_minmax(0,7fr)] lg:gap-16">
	<section class="flex flex-col gap-10 lg:pt-6">
		{#if store.connected}
			<div class="flex flex-col gap-4">
				<p class="eyebrow">Connected tag</p>
				<h1 class="display text-5xl break-all sm:text-6xl">{store.connectedDeviceName}</h1>
				<p class="text-soft">
					{store.deviceModelName} · {store.displayWidth}×{store.displayHeight}
				</p>
				<FirmwareStatus link />
			</div>

			<dl class="grid grid-cols-3 border-y border-line">
				{#snippet stat(label: string, value: string, unit: string)}
					<div
						class="flex flex-col gap-1 py-5 not-first:border-l not-first:border-line not-first:pl-5"
					>
						<dt class="eyebrow">{label}</dt>
						<dd class="text-2xl font-medium sm:text-3xl">
							{value}<span class="ml-1 text-base text-muted">{unit}</span>
						</dd>
					</div>
				{/snippet}
				{@render stat(
					'Temperature',
					store.temperatureC === null ? '—' : store.temperatureC.toFixed(1),
					'°C'
				)}
				{@render stat(
					'Battery',
					store.batteryPercent === null ? '—' : `${store.batteryPercent}`,
					'%'
				)}
				{@render stat('Clock set', formatTime(store.timeSyncedAt), '')}
			</dl>

			<DeviceSettings />
		{:else}
			<div class="flex flex-col gap-8">
				<p class="eyebrow">Hanshow Stellar e-paper tags</p>
				<h1 class="display text-5xl sm:text-7xl">E-paper,<br />from the browser.</h1>
				<p class="max-w-sm text-lg leading-relaxed text-soft">
					Connect a tag over Bluetooth to choose what it shows, send it pictures and update its
					firmware.
				</p>
			</div>
			{#if bluetoothAvailable}
				<button
					class="cta self-start"
					disabled={store.connecting}
					onclick={() => store.preConnect()}
				>
					{store.connecting ? 'Connecting…' : 'Connect a tag'}
				</button>
			{:else}
				<p class="max-w-sm border-l-2 border-accent pl-4 text-soft">
					This browser has no Web Bluetooth. Open the page in Chrome or Edge on a computer or an
					Android phone.
				</p>
			{/if}
		{/if}
	</section>

	<section class="flex min-w-0 flex-col gap-6" aria-label="Screens">
		{#if store.connected}
			<p class="text-soft">Choose what the tag shows.</p>
		{/if}
		<SceneCards
			interactive={store.connected && !store.busy}
			active={store.activeScene}
			onselect={(scene) => store.setScene(scene)}
		/>
	</section>
</div>
