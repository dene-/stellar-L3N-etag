<script lang="ts">
	import { onMount } from 'svelte';
	import type { FirmwareFile } from '#lib/firmware-file.ts';
	import { compareVersions, downloadFirmwareRelease } from '#lib/firmware-releases.ts';
	import { logStore } from '../../stores/logStore.svelte';
	import { releasesStore } from '../../stores/releasesStore.svelte';
	import Segmented from '../ui/Segmented.svelte';
	import FirmwareFilePicker from './FirmwareFilePicker.svelte';

	// A published release (bundled with the site) or a .bin file of your own; file is the chosen
	// image once it is loaded, null while there is none.
	type Props = {
		file: FirmwareFile | null;
		maxBytes: number;
		disabled?: boolean;
		installed?: string | null; // version on the tag, to mark it in the list
	};
	let { file = $bindable(), maxBytes, disabled = false, installed = null }: Props = $props();

	const releases = releasesStore;
	let mode = $state<'release' | 'file'>('release');
	let version = $state<string | null>(null);
	let ownFile = $state<FirmwareFile | null>(null);
	let loading = $state(false);

	onMount(async () => {
		await releases.load();
		if (releases.latest) await pickRelease(releases.latest.version);
		else mode = 'file';
	});

	async function pickRelease(next: string) {
		const release = releases.releases.find((entry) => entry.version === next);
		version = next;
		file = null;
		if (!release) return;
		loading = true;
		try {
			const downloaded = await downloadFirmwareRelease(release, maxBytes);
			if (mode === 'release' && version === next) file = downloaded;
		} catch (error) {
			logStore.addLog(error instanceof Error ? error.message : String(error));
		} finally {
			loading = false;
		}
	}

	function setMode(next: 'release' | 'file') {
		mode = next;
		if (next === 'file') file = ownFile;
		else if (version) void pickRelease(version);
	}

	function label(entry: (typeof releases.releases)[number], index: number) {
		const date = new Date(entry.date).toLocaleDateString([], {
			day: 'numeric',
			month: 'short',
			year: 'numeric'
		});
		const notes = [
			index === 0 ? 'latest' : '',
			installed && compareVersions(installed, entry.version) === 0 ? 'on the tag' : ''
		].filter(Boolean);
		return `${entry.version} · ${date}${notes.length ? ` (${notes.join(', ')})` : ''}`;
	}
</script>

<div class="flex flex-col gap-4">
	<Segmented
		label="Firmware source"
		options={[
			{ value: 'release', label: 'Release' },
			{ value: 'file', label: 'Own file' }
		]}
		value={mode}
		disabled={disabled || releases.status !== 'ready'}
		onchange={setMode}
	/>

	{#if mode === 'release'}
		<label class="flex flex-col gap-2">
			<span class="eyebrow">Version</span>
			<select
				class="field"
				value={version}
				disabled={disabled || releases.status !== 'ready'}
				onchange={(event) => pickRelease(event.currentTarget.value)}
			>
				{#each releases.releases as entry, index (entry.version)}
					<option value={entry.version}>{label(entry, index)}</option>
				{/each}
			</select>
		</label>
		<p class="text-sm text-muted" aria-live="polite">
			{#if releases.status === 'loading' || loading}
				Loading…
			{:else if file}
				{file.data.length.toLocaleString()} bytes ·
				<a
					class="text-accent underline-offset-4 hover:underline"
					href={releases.releases.find((entry) => entry.version === version)?.url}
					target="_blank"
					rel="noreferrer">What's new</a
				>
			{/if}
		</p>
	{:else}
		{#if releases.status === 'unavailable'}
			<p class="text-sm text-muted">The release list is unavailable; choose a .bin file.</p>
		{/if}
		<FirmwareFilePicker
			bind:file={ownFile}
			{maxBytes}
			{disabled}
			onpick={(picked) => (file = picked)}
		/>
	{/if}
</div>
