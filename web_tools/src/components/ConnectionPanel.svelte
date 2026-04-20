<script lang="ts">
	import { bleConnectionStore } from '../stores/connectionStore.svelte';
</script>

<div class="bg-base-300 rounded-lg p-3 prose max-w-none">
	<h3>Connection</h3>
	<div class="flex gap-3">
		<button
			class="btn btn-primary"
			onclick={() => bleConnectionStore.preConnect()}
			disabled={bleConnectionStore.isFlashingFirmware || bleConnectionStore.connected}
		>
			Connect
		</button>
		<button
			class="btn btn-secondary"
			onclick={() => bleConnectionStore.disconnect()}
			disabled={!bleConnectionStore.connected || bleConnectionStore.isFlashingFirmware}
		>
			Disconnect
		</button>
	</div>
	<div class="mt-3 text-sm opacity-80">
		<div>Device: {bleConnectionStore.connectedDeviceName || 'Not connected'}</div>
		<div>
			Display: {bleConnectionStore.deviceModelName} ({bleConnectionStore.displayWidth}x{bleConnectionStore.displayHeight})
			via {bleConnectionStore.displaySource}
		</div>
	</div>
</div>
