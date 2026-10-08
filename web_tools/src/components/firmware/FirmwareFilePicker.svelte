<script lang="ts">
	import { readFirmwareFile, type FirmwareFile } from '#lib/firmware-file.ts';
	import { logStore } from '../../stores/logStore.svelte';
	import FileDrop from '../ui/FileDrop.svelte';
	import Icon from '../ui/Icon.svelte';

	type Props = { file: FirmwareFile | null; maxBytes: number; disabled?: boolean };
	let { file = $bindable(), maxBytes, disabled = false }: Props = $props();

	async function pick(files: File[]) {
		try {
			file = await readFirmwareFile(files[0], maxBytes);
			logStore.addLog(`${file.name} selected, ${file.data.length} bytes.`);
		} catch (error) {
			file = null;
			logStore.addLog(error instanceof Error ? error.message : String(error));
		}
	}
</script>

<FileDrop accept=".bin" {disabled} class="min-h-24 px-5 py-4" onfiles={pick}>
	{#snippet children({ dragging })}
		{#if file}
			<span class="text-fg">{file.name}</span>
			<span class="text-sm text-muted"
				>{file.data.length.toLocaleString()} bytes · click to change</span
			>
		{:else}
			<span class="flex items-center gap-2 text-soft">
				<Icon name="upload" size={18} class="text-accent" />
				{dragging ? 'Drop the file' : 'Choose or drop a .bin file'}
			</span>
		{/if}
	{/snippet}
</FileDrop>
