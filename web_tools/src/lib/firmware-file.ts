export type FirmwareFile = { name: string; data: Uint8Array };

// The Telink header: "KNLT" at offset 8 and the image's own size (_bin_size_) at 0x18, little-endian
// (see cstartup_825x.S).
const MAGIC_OFFSET = 8;
const SIZE_OFFSET = 0x18;

// Checks a firmware image is a Telink one that fits and is complete: its header size is set, is not
// more than the file holds and is within the limit.
export function checkFirmwareImage(name: string, data: Uint8Array, maxBytes: number): FirmwareFile {
	if (data.length > maxBytes) {
		throw new Error(`${name} is ${data.length} bytes; the limit is ${maxBytes}.`);
	}
	if (
		data.length < SIZE_OFFSET + 4 ||
		String.fromCharCode(...data.subarray(MAGIC_OFFSET, MAGIC_OFFSET + 4)) !== 'KNLT'
	) {
		throw new Error(`${name} is not a firmware image for this tag.`);
	}
	const binSize = new DataView(data.buffer, data.byteOffset, data.byteLength).getUint32(
		SIZE_OFFSET,
		true
	);
	if (binSize === 0) {
		throw new Error(`${name} has no size in its header; it is not a complete firmware image.`);
	}
	if (binSize > data.length) {
		throw new Error(
			`${name} is cut short: its header says ${binSize} bytes, the file has ${data.length}.`
		);
	}
	if (binSize > maxBytes) {
		throw new Error(`${name} is ${binSize} bytes by its header; the limit is ${maxBytes}.`);
	}
	return { name, data };
}

export async function readFirmwareFile(file: File, maxBytes: number): Promise<FirmwareFile> {
	return checkFirmwareImage(file.name, new Uint8Array(await file.arrayBuffer()), maxBytes);
}
