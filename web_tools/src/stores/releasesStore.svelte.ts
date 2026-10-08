import {
	compareVersions,
	fetchFirmwareReleases,
	type FirmwareRelease
} from '#lib/firmware-releases.ts';
import { logStore } from './logStore.svelte';

class ReleasesStore {
	// Newest first.
	releases: FirmwareRelease[] = $state([]);
	status: 'idle' | 'loading' | 'ready' | 'unavailable' = $state('idle');

	get latest(): FirmwareRelease | null {
		return this.releases[0] ?? null;
	}

	async load() {
		if (this.status === 'loading' || this.status === 'ready') return;
		this.status = 'loading';
		try {
			this.releases = await fetchFirmwareReleases();
			this.status = 'ready';
		} catch (error) {
			this.status = 'unavailable';
			logStore.addLog(
				`Firmware releases: ${error instanceof Error ? error.message : String(error)}`
			);
		}
	}

	// The latest release if the tag runs an older firmware, else null. installed is null for
	// firmware before v0.10.0, which does not report its version; a local build ("dev") is never
	// called outdated.
	updateFor(installed: string | null): FirmwareRelease | null {
		const latest = this.latest;
		if (!latest) return null;
		if (installed === null) return latest;
		const order = compareVersions(installed, latest.version);
		return order !== null && order < 0 ? latest : null;
	}
}

export const releasesStore = new ReleasesStore();
