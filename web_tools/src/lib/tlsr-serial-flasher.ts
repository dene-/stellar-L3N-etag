type SerialPortLike = {
	open(options: {
		baudRate: number;
		bufferSize?: number;
		baudrate?: number;
		buffersize?: number;
	}): Promise<void>;
	close(): Promise<void>;
	setSignals(signals: { dataTerminalReady: boolean; requestToSend: boolean }): Promise<void>;
	writable: WritableStream<Uint8Array>;
};

type NavigatorWithSerial = Navigator & {
	serial?: {
		requestPort(): Promise<SerialPortLike>;
	};
};

type SerialFlasherCallbacks = {
	log: (message: string) => void;
	onStatus?: (message: string) => void;
	onProgress?: (percent: number) => void;
	onConnectionChange?: (connected: boolean) => void;
};

function delay(ms: number) {
	return new Promise<void>((resolve) => {
		setTimeout(resolve, ms);
	});
}

function hex(number: number, length: number) {
	let out = number.toString(16).toUpperCase();
	while (out.length < length) {
		out = '0' + out;
	}
	return out;
}

export class TlsrSerialFlasher {
	private port: SerialPortLike | null = null;
	private writer: WritableStreamDefaultWriter<Uint8Array> | null = null;

	constructor(private callbacks: SerialFlasherCallbacks) {}

	static isSupported() {
		return 'serial' in navigator;
	}

	get isOpen() {
		return this.port !== null && this.writer !== null;
	}

	async open(baudRate: number) {
		const serialApi = (navigator as NavigatorWithSerial).serial;

		if (!serialApi) {
			throw new Error('Web Serial is not available in this browser.');
		}

		this.port = await serialApi.requestPort();

		await this.port.open({
			baudRate,
			baudrate: baudRate,
			bufferSize: 240,
			buffersize: 240
		});

		this.writer = this.port.writable.getWriter();
		await this.port.setSignals({ dataTerminalReady: false, requestToSend: false });

		this.callbacks.log('USB-COM opened.');
		this.callbacks.onConnectionChange?.(true);
	}

	async close() {
		try {
			await this.writer?.close();
		} finally {
			this.writer = null;
			if (this.port) {
				await this.port.close();
				this.port = null;
			}
			this.callbacks.log('USB-COM closed.');
			this.callbacks.onConnectionChange?.(false);
		}
	}

	async flashFirmware(firmware: Uint8Array, activationTimeMs: number) {
		this.requireOpen();

		const start = Date.now();

		this.updateProgress(0, 'Preparing flash...');
		await this.activate(activationTimeMs);
		await this.flashWriteEnable();
		await this.flashUnlock();
		await delay(1500);

		this.callbacks.log(`Write ${firmware.byteLength} bytes in to Flash...`);

		let address = 0;
		let remaining = firmware.byteLength;
		let blockSize = 256;

		while (remaining > 0) {
			if ((address & 0x0fff) === 0) {
				this.updateProgress(
					Math.floor((address / firmware.byteLength) * 100),
					`Flash Sector Erase at 0x${hex(address, 6)}`
				);
				await this.sectorErase(address);
			}

			if (remaining < blockSize) {
				blockSize = remaining;
			}

			this.updateProgress(
				Math.floor((address / firmware.byteLength) * 100),
				`Flash Write ${blockSize} bytes at 0x${hex(address, 6)}`
			);

			await this.writeFlashBlock(address, firmware.slice(address, address + blockSize));

			address += blockSize;
			remaining -= blockSize;
		}

		this.updateProgress(100, 'ok');

		this.callbacks.log(`Done (${((Date.now() - start) / 1000).toFixed(3)} sec).`);
		this.callbacks.log('Soft Reset MCU');

		await this.softReset();
	}

	async unlockFlash(activationTimeMs: number) {
		this.requireOpen();
		await this.activate(activationTimeMs);
		this.updateProgress(100, 'Unlocked!');
		this.callbacks.log('Flash Erase All (3.5 sec)...');
		await this.flashWriteEnable();
		await this.flashUnlock();
		await delay(3500);
		this.callbacks.log('Done.');
	}

	async eraseAllFlash(activationTimeMs: number) {
		this.requireOpen();
		await this.activate(activationTimeMs);
		this.updateProgress(100, 'Erased!');
		this.callbacks.log('Flash Erase All (3.5 sec)...');
		await this.flashWriteEnable();
		await this.flashEraseAll();
		await delay(3500);
		this.callbacks.log('Done.');
	}

	async softResetWithActivation(activationTimeMs: number) {
		this.requireOpen();
		await this.activate(activationTimeMs);
		this.updateProgress(0, '');
		this.callbacks.log('Soft Reset MCU');
		await this.softReset();
		this.callbacks.log('Done.');
	}

	private requireOpen() {
		if (!this.port || !this.writer) {
			throw new Error('Open the serial port first.');
		}
	}

	private updateProgress(percent: number, message: string) {
		this.callbacks.onProgress?.(percent);
		this.callbacks.onStatus?.(message);
	}

	private async writeRaw(data: Uint8Array) {
		this.requireOpen();

		return this.writer!.write(data);
	}

	private swsWriteAddress(address: number, data: Uint8Array) {
		const bytePattern = new Uint8Array(10);
		const header = new Uint8Array([
			0x5a,
			(address >> 16) & 0xff,
			(address >> 8) & 0xff,
			address & 0xff,
			0x00
		]);
		const packet = new Uint8Array((data.byteLength + 6) * 10);

		bytePattern[0] = 0x80;
		bytePattern[9] = 0xfe;

		header.forEach((value, index) => {
			let mask = 0x80;
			let offset = 1;

			do {
				bytePattern[offset] = (value & mask) !== 0 ? 0x80 : 0xfe;
				offset += 1;
				mask >>= 1;
			} while (mask !== 0);

			packet.set(bytePattern, index * 10);
			bytePattern[0] = 0xfe;
		});

		data.forEach((value, index) => {
			let mask = 0x80;
			let offset = 1;

			do {
				bytePattern[offset] = (value & mask) !== 0 ? 0x80 : 0xfe;
				offset += 1;
				mask >>= 1;
			} while (mask !== 0);

			packet.set(bytePattern, (index + 5) * 10);
		});

		bytePattern.fill(0x80, 0, 9);
		packet.set(bytePattern, (data.byteLength + 5) * 10);

		return packet;
	}

	private async flashByteCommand(command: number) {
		await this.writeRaw(this.swsWriteAddress(0x0d, new Uint8Array([0x00])));

		return this.writeRaw(this.swsWriteAddress(0x0c, new Uint8Array([command & 0xff, 0x01])));
	}

	private async flashWriteEnable() {
		return this.flashByteCommand(0x06);
	}

	private async flashWakeUp() {
		return this.flashByteCommand(0xab);
	}

	private async flashUnlock() {
		await this.flashWriteEnable();

		await this.writeRaw(this.swsWriteAddress(0x0d, new Uint8Array([0x00])));
		await this.writeRaw(this.swsWriteAddress(0x0c, new Uint8Array([0x01])));
		await this.writeRaw(this.swsWriteAddress(0x0c, new Uint8Array([0x00, 0x01])));

		await this.flashWriteEnable();
		await this.writeRaw(this.swsWriteAddress(0x0d, new Uint8Array([0x00])));
		await this.writeRaw(this.swsWriteAddress(0x0c, new Uint8Array([0x01])));
		await this.writeRaw(this.swsWriteAddress(0x0c, new Uint8Array([0x00])));

		return this.writeRaw(this.swsWriteAddress(0x0c, new Uint8Array([0x00, 0x01])));
	}

	private async flashEraseAll() {
		return this.flashByteCommand(0x60);
	}

	private async writeFifo(address: number, data: Uint8Array) {
		await this.writeRaw(this.swsWriteAddress(0x00b3, new Uint8Array([0x80])));
		await this.writeRaw(this.swsWriteAddress(address, data));
		return this.writeRaw(this.swsWriteAddress(0x00b3, new Uint8Array([0x00])));
	}

	private async sectorErase(address: number) {
		await this.flashWriteEnable();

		await this.writeRaw(this.swsWriteAddress(0x0d, new Uint8Array([0x00])));
		await this.writeRaw(this.swsWriteAddress(0x0c, new Uint8Array([0x20])));
		await this.writeRaw(this.swsWriteAddress(0x0c, new Uint8Array([(address >> 16) & 0xff])));
		await this.writeRaw(this.swsWriteAddress(0x0c, new Uint8Array([(address >> 8) & 0xff])));
		await this.writeRaw(this.swsWriteAddress(0x0c, new Uint8Array([address & 0xff, 0x01])));

		await delay(300);
	}

	private async writeFlashBlock(address: number, data: Uint8Array) {
		await this.flashWriteEnable();
		await this.writeRaw(this.swsWriteAddress(0x0d, new Uint8Array([0x00])));

		const block = new Uint8Array(4 + data.byteLength);

		block[0] = 0x02;
		block[1] = (address >> 16) & 0xff;
		block[2] = (address >> 8) & 0xff;
		block[3] = address & 0xff;
		block.set(data, 4);

		await this.writeFifo(0x0c, block);
		await this.writeRaw(this.swsWriteAddress(0x0d, new Uint8Array([0x01])));

		await delay(10);
	}

	private async softReset() {
		await this.writeRaw(this.swsWriteAddress(0x06f, new Uint8Array([0x20])));
	}

	private async activate(activationTimeMs: number) {
		const stopCpuBlock = this.swsWriteAddress(0x0602, new Uint8Array([0x05]));

		this.updateProgress(0, 'Reset DTR/RTS (100 ms)');
		this.callbacks.log('Reset DTR/RTS (100 ms)');

		await this.reset(100);
		await this.softReset();

		this.updateProgress(0, `Activate (${(activationTimeMs / 1000).toFixed(1)} sec)...`);
		this.callbacks.log(`Activate (${activationTimeMs / 1000} sec)...`);

		const start = Date.now();

		while (Date.now() - start < activationTimeMs) {
			await this.writeRaw(stopCpuBlock);
		}

		await this.writeRaw(this.swsWriteAddress(0x00b2, new Uint8Array([55])));
		await this.writeRaw(stopCpuBlock);

		await this.flashWakeUp();
	}

	private async reset(durationMs: number) {
		this.requireOpen();

		await this.port!.setSignals({ dataTerminalReady: true, requestToSend: true });
		await delay(durationMs);
		await this.port!.setSignals({ dataTerminalReady: false, requestToSend: false });
	}
}
