const TABLE = new Uint32Array(256).map((_, index) => {
	let value = index;
	for (let bit = 0; bit < 8; bit++) {
		value = value & 1 ? 0xedb88320 ^ (value >>> 1) : value >>> 1;
	}
	return value >>> 0;
});

// Standard reflected CRC-32 (zlib, PNG). Pass the previous result as `crc` to continue over more
// data: crc32(b, crc32(a)) equals crc32(a followed by b).
export function crc32(data: Uint8Array, crc = 0): number {
	let value = ~crc;
	for (const byte of data) {
		value = TABLE[(value ^ byte) & 0xff] ^ (value >>> 8);
	}
	return ~value >>> 0;
}
