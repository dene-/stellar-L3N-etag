import { checkFirmwareImage, type FirmwareFile } from '#lib/firmware-file.ts';

// Firmware releases bundled into the site by tools/bundle_firmware_releases.py (browsers can't
// download GitHub release assets), newest first.
export type FirmwareRelease = {
	version: string; // "0.10.0"
	tag: string;
	date: string; // ISO 8601
	file: string; // relative to firmware/
	size: number;
	url: string; // release page on GitHub
};

function siteUrl(path: string) {
	return new URL(`firmware/${path}`, document.baseURI);
}

export async function fetchFirmwareReleases(): Promise<FirmwareRelease[]> {
	const response = await fetch(siteUrl('index.json'), { cache: 'no-cache' });
	if (!response.ok) throw new Error(`The release list is unavailable (HTTP ${response.status}).`);
	return response.json();
}

// The image of a release, named "v0.10.0".
export async function downloadFirmwareRelease(
	release: FirmwareRelease,
	maxBytes: number
): Promise<FirmwareFile> {
	const response = await fetch(siteUrl(release.file));
	if (!response.ok) throw new Error(`Could not download ${release.tag} (HTTP ${response.status}).`);
	return checkFirmwareImage(
		`v${release.version}`,
		new Uint8Array(await response.arrayBuffer()),
		maxBytes
	);
}

// Major, minor, patch and commits after that release ("0.9.0-3-gabc1234" from a local build).
function versionParts(version: string): number[] | null {
	const match = /^v?(\d+)\.(\d+)\.(\d+)(?:-(\d+)-g[0-9a-f]+)?$/.exec(version.trim());
	return match ? match.slice(1).map((part) => Number(part ?? 0)) : null;
}

// Negative if a is older than b, 0 if equal, positive if newer; null if either is not a version.
export function compareVersions(a: string, b: string): number | null {
	const pa = versionParts(a);
	const pb = versionParts(b);
	if (!pa || !pb) return null;
	for (let i = 0; i < pa.length; i++) {
		if (pa[i] !== pb[i]) return pa[i] - pb[i];
	}
	return 0;
}
