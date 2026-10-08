export type FirmwareFile = { name: string; data: Uint8Array };

// Reads a firmware image and checks it is a Telink one: "KNLT" at offset 8.
export async function readFirmwareFile(file: File, maxBytes: number): Promise<FirmwareFile> {
	if (file.size > maxBytes) {
		throw new Error(`${file.name} is ${file.size} bytes; the limit is ${maxBytes}.`);
	}
	const data = new Uint8Array(await file.arrayBuffer());
	const signature = String.fromCharCode(...data.subarray(8, 12));
	if (signature !== 'KNLT') {
		throw new Error(`${file.name} is not a firmware image for this tag.`);
	}
	return { name: file.name, data };
}
