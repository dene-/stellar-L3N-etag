<script lang="ts">
	import ConnectionPanel from '../components/ConnectionPanel.svelte';
	import EpdControlPanel from '../components/EpdControlPanel.svelte';
	import ImageUploadPanel from '../components/ImageUploadPanel.svelte';
	import FirmwarePanel from '../components/FirmwarePanel.svelte';
	import SerialFirmwarePanel from '../components/SerialFirmwarePanel.svelte';
	import LogPanel from '../components/LogPanel.svelte';
</script>

<div class="flex flex-col flex-grow lg:flex-row gap-3 pb-5">
	<div class="flex-grow flex flex-col gap-3">
		<ConnectionPanel />
		<EpdControlPanel />
		<ImageUploadPanel />
		<FirmwarePanel />
		<SerialFirmwarePanel />
	</div>
	<LogPanel />
</div>
