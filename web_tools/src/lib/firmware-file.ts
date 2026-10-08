export type FirmwareFile = { name: string; data: Uint8Array };

// Checks a firmware image fits and is a Telink one: "KNLT" at offset 8.
export function checkFirmwareImage(name: string, data: Uint8Array, maxBytes: number): FirmwareFile {
	if (data.length > maxBytes) {
		throw new Error(`${name} is ${data.length} bytes; the limit is ${maxBytes}.`);
	}
	if (String.fromCharCode(...data.subarray(8, 12)) !== 'KNLT') {
		throw new Error(`${name} is not a firmware image for this tag.`);
	}
	return { name, data };
}

export async function readFirmwareFile(file: File, maxBytes: number): Promise<FirmwareFile> {
	return checkFirmwareImage(file.name, new Uint8Array(await file.arrayBuffer()), maxBytes);
}
