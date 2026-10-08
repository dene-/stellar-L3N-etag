// Declare globals that may be provided elsewhere
import { logStore } from './logStore.svelte';
import { intToHex, bytesToHex, hexToBytes } from '$lib/utils';
import { FLASH_IMAGE_STORAGE_BYTES } from '$lib/photo-utils';

type DisplaySource = 'default' | 'firmware' | 'manual' | 'name';

type DisplayModelInfo = {
	model: number;
	name: string;
	width: number;
	height: number;
};

type StoredImageBuffers = {
	name: string;
	black: Uint8Array;
	red: Uint8Array;
};

export const DISPLAY_MODEL_OPTIONS: DisplayModelInfo[] = [
	{ model: 0, name: 'Auto detect (2.9" or 2.13" BWR)', width: 250, height: 128 },
	{ model: 1, name: 'BW213', width: 250, height: 128 },
	{ model: 2, name: 'BWR213', width: 250, height: 128 },
	{ model: 3, name: 'BWR154', width: 200, height: 200 },
	{ model: 4, name: '213ICE', width: 212, height: 104 },
	{ model: 5, name: 'BWR290 / BWR296', width: 296, height: 128 },
	{ model: 6, name: 'BW290 / BW296', width: 296, height: 128 }
];

const DISPLAY_MODEL_MAP = new Map(DISPLAY_MODEL_OPTIONS.map((info) => [info.model, info]));

// Assumed until the device reports its model (E2 AB) after connecting.
const DEFAULT_DISPLAY_INFO = DISPLAY_MODEL_MAP.get(2)!;

// OTA layout, mirrors OTA_BANK_START/OTA_MAX_SIZE in Firmware/src/ble/ota_service.c. The last 256 byte
// page of the bank is never written by the device, so the largest image is one page shorter.
export const OTA_BANK_ADDRESS = 0x20000;
const OTA_BANK_SIZE = 0x20000;
export const OTA_MAX_FIRMWARE_SIZE = OTA_BANK_SIZE - 0x100;
// Rewriting 128 KiB takes a few seconds; the link loss is only noticed after the supervision
// timeout the firmware requests (20 s), so allow comfortably more than both.
const OTA_REBOOT_TIMEOUT_MS = 45000;

function resolveDisplayModel(model: number): DisplayModelInfo {
	return DISPLAY_MODEL_MAP.get(model) ?? DEFAULT_DISPLAY_INFO;
}

function inferDisplayModelFromName(name: string | null | undefined): DisplayModelInfo | null {
	if (!name) {
		return null;
	}

	const normalizedName = name.toLowerCase();

	if (/\b(290|296|2\.9)\b/.test(normalizedName)) {
		return resolveDisplayModel(5);
	}

	if (/\b(213|250|122|2\.13)\b/.test(normalizedName)) {
		return resolveDisplayModel(2);
	}

	return null;
}

class BleConnectionStore {
	private bleDevice: BluetoothDevice | null = $state(null);
	private gattServer: BluetoothRemoteGATTServer | null = $state(null);
	private rxtxService: BluetoothRemoteGATTService | null = $state(null);
	private rxtxCharacteristic: BluetoothRemoteGATTCharacteristic | null = $state(null);
	private epdService: BluetoothRemoteGATTService | null = $state(null);
	private epdCharacteristic: BluetoothRemoteGATTCharacteristic | null = $state(null);
	private writeService: BluetoothRemoteGATTService | null = $state(null);
	private writeCharacteristic: BluetoothRemoteGATTCharacteristic | null = $state(null);
	private suppressE5Notifications = false;

	private bleDeviceOptionalServicesIds: string[] = [
		'0000221f-0000-1000-8000-00805f9b34fb',
		'00001f10-0000-1000-8000-00805f9b34fb',
		'13187b10-eba9-a3ba-044e-83d3217d9a38'
	];
	private rxtxServiceId = '00001f10-0000-1000-8000-00805f9b34fb';
	private rxtxCharacteristicId = '00001f1f-0000-1000-8000-00805f9b34fb';
	private edpServiceId = '13187b10-eba9-a3ba-044e-83d3217d9a38';
	private edpCharacteristicId = '4b646063-6264-f3a7-8941-e65356ea82fe';
	private writeServiceId = '0000221f-0000-1000-8000-00805f9b34fb';
	private writeCharacteristicId = '0000331f-0000-1000-8000-00805f9b34fb';

	preconnected = $state(false);
	connected = $state(false);
	firmwareUploadProgress = $state(0);
	imageUploadProgress = $state(0);
	isFlashingFirmware = $state(false);
	isUploadingImages = $state(false);
	connectedDeviceName = $state('');
	// Panel the device drives (resolved by detection); sizes rendered images.
	deviceModel = $state(DEFAULT_DISPLAY_INFO.model);
	// Model chosen on the device (0 = auto-detect), shown in the display model selector.
	selectedModel = $state(DEFAULT_DISPLAY_INFO.model);
	deviceModelName = $state(DEFAULT_DISPLAY_INFO.name);
	displayWidth = $state(DEFAULT_DISPLAY_INFO.width);
	displayHeight = $state(DEFAULT_DISPLAY_INFO.height);
	fastRefreshEnabled = $state(false);
	fastRefreshSupported = $state(false);
	displaySource: DisplaySource = $state('default');

	private applyDisplayModelInfo(info: DisplayModelInfo, source: DisplaySource) {
		this.deviceModel = info.model;
		this.deviceModelName = info.name;
		this.displayWidth = info.width;
		this.displayHeight = info.height;
		this.displaySource = source;
	}

	private applyDisplayGeometry(
		model: number,
		width: number,
		height: number,
		source: DisplaySource
	) {
		const info = resolveDisplayModel(model);

		this.deviceModel = model;
		this.deviceModelName = info.name;
		this.displayWidth = width || info.width;
		this.displayHeight = height || info.height;
		this.displaySource = source;
	}

	// The device's gattserverdisconnected event; one stable function so it is registered once.
	private readonly onGattDisconnected = () => {
		this.resetVariables();
		logStore.addLog('Disconnected.');
	};

	disconnect() {
		if (this.bleDevice?.gatt?.connected) {
			this.bleDevice.gatt.disconnect(); // fires gattserverdisconnected, which resets the state
		} else {
			this.onGattDisconnected();
		}
	}

	async preConnect() {
		if (this.bleDevice?.gatt?.connected) {
			this.disconnect();
			return;
		}

		try {
			const device = await navigator.bluetooth.requestDevice({
				optionalServices: this.bleDeviceOptionalServicesIds,
				acceptAllDevices: true
			});
			this.bleDevice?.removeEventListener('gattserverdisconnected', this.onGattDisconnected);
			device.addEventListener('gattserverdisconnected', this.onGattDisconnected);
			this.bleDevice = device;

			this.connectedDeviceName = device.name ?? 'Unknown device';

			const inferredDisplay = inferDisplayModelFromName(device.name);
			if (inferredDisplay) {
				this.applyDisplayModelInfo(inferredDisplay, 'name');
			}

			this.preconnected = true;
			await this.connect();
		} catch (e) {
			// A half-finished connection (e.g. a device without these services) must not stay open.
			if (this.bleDevice?.gatt?.connected) {
				this.bleDevice.gatt.disconnect();
			}
			this.resetVariables();
			await handleError(e);
		}
	}

	async connect() {
		if (this.epdCharacteristic || !this.bleDevice) {
			return;
		}

		logStore.addLog('Connecting to: ' + this.bleDevice.name);

		if (!this.bleDevice.gatt) {
			throw new Error('No BLE device selected.');
		}

		this.gattServer = await this.bleDevice.gatt.connect();
		logStore.addLog('Found GATT server.');

		this.epdService = await this.gattServer.getPrimaryService(this.edpServiceId);
		logStore.addLog('Found EDP service.');

		this.epdCharacteristic = await this.epdService.getCharacteristic(this.edpCharacteristicId);
		logStore.addLog('EDP Service connected.');

		await this.epdCharacteristic.startNotifications();

		this.epdCharacteristic.addEventListener('characteristicvaluechanged', (event: Event) => {
			const characteristic = event.target as BluetoothRemoteGATTCharacteristic;

			const value = characteristic.value;

			if (!value) {
				logStore.addLog('[From display]: No data.');
				return;
			}

			const hex = bytesToHex(value.buffer);

			const count = parseInt('0x' + hex);

			logStore.addLog(`[From display]: Received ${count} bytes.`);
		});

		this.writeService = await this.gattServer.getPrimaryService(this.writeServiceId);
		logStore.addLog('Found Write service.');

		this.writeCharacteristic = await this.writeService.getCharacteristic(
			this.writeCharacteristicId
		);
		logStore.addLog('Write Service connected.');

		// document.getElementById('connectbutton').innerHTML = 'Disconnect';
		await this.connectRXTX();
	}

	async connectRXTX() {
		if (!this.gattServer) {
			throw new Error('No GATT server available.');
		}

		this.rxtxService = await this.gattServer.getPrimaryService(this.rxtxServiceId);
		logStore.addLog('Found RXTX service.');

		this.rxtxCharacteristic = await this.rxtxService.getCharacteristic(this.rxtxCharacteristicId);
		logStore.addLog('RXTX Service connected.');

		// Start notifications to receive async responses from the device (e.g. E2 AA temperature)
		await this.rxtxCharacteristic.startNotifications();

		this.rxtxCharacteristic.addEventListener('characteristicvaluechanged', (event: Event) => {
			const characteristic = event.target as BluetoothRemoteGATTCharacteristic;

			const value = characteristic.value;

			if (!value) {
				logStore.addLog('[From display]: No data.');
				return;
			}

			const data = new Uint8Array(value.buffer, value.byteOffset, value.byteLength);

			// Suppress E5 notifications during image upload (handled by upload listener)
			if (this.suppressE5Notifications && data[0] === 0xe5) {
				return;
			}

			if (data.byteLength >= 7 && data[0] === 0xe2 && data[1] === 0xab) {
				const model = data[2];
				const width = data[3] | (data[4] << 8);
				const height = data[5] | (data[6] << 8);

				this.applyDisplayGeometry(model, width, height, 'firmware');
				// Firmware before v0.7.1 sends 7 bytes and doesn't say whether the model was auto-detected.
				this.selectedModel = data.byteLength >= 8 ? data[7] : model;
				logStore.addLog(
					`[From display][RXTX]: ${this.deviceModelName} ${this.displayWidth}x${this.displayHeight}`
				);
				return;
			}

			if (data.byteLength === 3 && data[0] === 0xe6) {
				this.fastRefreshEnabled = data[1] === 0x01;
				this.fastRefreshSupported = data[2] === 0x01;
				logStore.addLog(
					`[From display][RXTX]: Fast refresh ${this.fastRefreshEnabled ? 'enabled' : 'disabled'}${this.fastRefreshSupported ? '' : ' (not supported by current panel)'}`
				);
				return;
			}

			const hex = bytesToHex(data);

			// Firmware sends 2 bytes: int16 LE (temp * 10). If no decimals, it's in steps of 10.
			if (value.byteLength === 2) {
				const t10 = value.getInt16(0, true);
				const tempC = Math.round(t10 / 10);
				logStore.addLog(`[From display][RXTX]: Temperature ${tempC}°C`);
				return;
			}

			// Fallback: log raw payload
			logStore.addLog(`[From display][RXTX]: ${hex}`);
		});

		this.connected = true;

		// Allow BLE connection parameters and CCCD writes to stabilise
		// before querying the device, otherwise the firmware may silently
		// drop the notification response.
		await new Promise((r) => setTimeout(r, 600));
		await this.queryDisplayInfo();
	}

	async queryDisplayInfo() {
		if (!this.rxtxCharacteristic) {
			logStore.addLog('Service unavailable. Is Bluetooth connected?');
			return;
		}

		logStore.addLog('Querying display model...');
		await this.sendRxTxCommand('e2ab');
		await this.queryFastRefreshInfo();
	}

	async queryFastRefreshInfo() {
		if (!this.rxtxCharacteristic) {
			logStore.addLog('Service unavailable. Is Bluetooth connected?');
			return;
		}

		logStore.addLog('Querying fast refresh state...');
		await this.sendRxTxCommand('e6aa');
	}

	async setFastRefreshEnabled(enabled: boolean) {
		await this.sendRxTxCommand(enabled ? 'e601' : 'e600');
	}

	async setDisplayModel(model: number) {
		this.selectedModel = model;
		if (model !== 0) {
			this.applyDisplayModelInfo(resolveDisplayModel(model), 'manual');
		}

		await this.sendRxTxCommand(`e0${intToHex(model, 1)}`);
		await this.queryDisplayInfo();
	}

	// Errors are logged, not thrown: the UI buttons call this without handling failures.
	async sendRxTxCommand(command: string) {
		if (!this.rxtxCharacteristic) {
			logStore.addLog('Service unavailable. Is Bluetooth connected?');
			return;
		}
		logStore.addLog(`Sending RXTX command: ${command}`);
		try {
			await this.rxtxCharacteristic.writeValueWithResponse(hexToBytes(command) as BufferSource);
		} catch (e) {
			await handleError(e);
		}
	}

	async uploadImageSet(images: StoredImageBuffers[], intervalSeconds: number): Promise<void> {
		// Held for the whole upload: a dropped link makes its writes fail instead of hitting null.
		const rxtx = this.rxtxCharacteristic;
		if (!rxtx) {
			logStore.addLog('Service unavailable. Is Bluetooth connected?');
			return;
		}

		if (!images.length) {
			logStore.addLog('No images selected.');
			return;
		}

		const planeSize = images[0].black.length;
		const totalBytes = planeSize * 2 * images.length;
		const uploadModel = this.deviceModel || DEFAULT_DISPLAY_INFO.model;
		const chunkSize = 240;
		// Sent as uint16 seconds; the device treats 0 as 60 s.
		const slideshowInterval =
			images.length > 1 ? Math.min(0xffff, Math.max(1, Math.round(intervalSeconds) || 60)) : 0;

		if (totalBytes > FLASH_IMAGE_STORAGE_BYTES) {
			throw new Error('Selected images exceed the MCU flash space reserved for photos.');
		}

		for (const image of images) {
			if (image.black.length !== planeSize || image.red.length !== planeSize) {
				throw new Error('All rendered images must share the same display size.');
			}
		}

		this.isFlashingFirmware = true;
		this.isUploadingImages = true;
		this.imageUploadProgress = 0;
		this.suppressE5Notifications = true;
		let uploadAborted = false;

		// The device only answers a chunk write (E5 01) when it rejects it: E5 01 00.
		const chunkFailureListener = (event: Event) => {
			const v = (event.target as BluetoothRemoteGATTCharacteristic).value;
			if (!v || v.byteLength < 3) return;
			const d = new Uint8Array(v.buffer, v.byteOffset, v.byteLength);
			if (d[0] === 0xe5 && d[1] === 0x01 && d[2] === 0x00) {
				logStore.addLog('Device rejected an image chunk.');
				uploadAborted = true;
			}
		};
		rxtx.addEventListener('characteristicvaluechanged', chunkFailureListener);
		const isReplyTo = (subcommand: number) => (d: Uint8Array) =>
			d.length === 3 && d[0] === 0xe5 && d[1] === subcommand;

		try {
			const start = Date.now();
			logStore.addLog(`Preparing persistent upload for ${images.length} image(s)...`);

			// E5 00: the device erases its image flash (a few seconds) and replies E5 00 <ok>.
			const prepared = await this.writeAndAwaitReply(
				rxtx,
				new Uint8Array([
					0xe5,
					0x00,
					uploadModel,
					images.length,
					slideshowInterval & 0xff,
					(slideshowInterval >> 8) & 0xff
				]),
				isReplyTo(0x00),
				15000
			);
			if (!prepared || prepared[2] !== 0x01) {
				logStore.addLog(
					prepared
						? 'Device rejected the prepare command (model or image count). Upload aborted.'
						: 'Device did not answer the prepare command. Upload aborted.'
				);
				return;
			}

			const totalChunks = images.length * 2 * Math.ceil(planeSize / chunkSize);
			let chunksDone = 0;

			for (const [index, image] of images.entries()) {
				if (uploadAborted) break;

				for (const [plane, buffer] of [image.black, image.red].entries()) {
					if (uploadAborted) break;

					for (let offset = 0; offset < buffer.length; offset += chunkSize) {
						if (uploadAborted) break;

						const chunk = buffer.slice(offset, offset + chunkSize);
						const packet = new Uint8Array(6 + chunk.length);

						packet[0] = 0xe5;
						packet[1] = 0x01;
						packet[2] = index;
						packet[3] = plane;
						packet[4] = offset & 0xff;
						packet[5] = (offset >> 8) & 0xff;
						packet.set(chunk, 6);

						await rxtx.writeValueWithResponse(packet);
						chunksDone++;
						this.imageUploadProgress = (chunksDone / totalChunks) * 100;
					}
				}
				logStore.addLog(
					`Image ${index + 1}/${images.length} sent (${Math.round(this.imageUploadProgress)}%)`
				);
			}

			if (uploadAborted) {
				logStore.addLog('Upload aborted due to device error. Images may be corrupted.');
				return;
			}

			// E5 02: the device commits the upload, replies E5 02 <ok> and starts showing it.
			const committed = await this.writeAndAwaitReply(
				rxtx,
				new Uint8Array([0xe5, 0x02]),
				isReplyTo(0x02),
				5000
			);
			if (!committed || committed[2] !== 0x01) {
				logStore.addLog('Device did not confirm the upload. Upload it again.');
				return;
			}
			const elapsed = ((Date.now() - start) / 1000).toFixed(1);

			this.imageUploadProgress = 100;
			logStore.addLog(
				images.length > 1
					? `Slideshow uploaded in ${elapsed}s. Interval: ${slideshowInterval}s`
					: `Photo uploaded in ${elapsed}s (persistent single-photo mode).`
			);
		} finally {
			rxtx.removeEventListener('characteristicvaluechanged', chunkFailureListener);
			this.isFlashingFirmware = false;
			this.isUploadingImages = false;
			this.suppressE5Notifications = false;
		}
	}

	private async eraseFwArea(ota: BluetoothRemoteGATTCharacteristic) {
		const totalSectors = OTA_BANK_SIZE / 0x1000;
		for (let sector = 0; sector < totalSectors; sector++) {
			const address = OTA_BANK_ADDRESS + sector * 0x1000;
			const pkt = new Uint8Array(5);
			pkt[0] = 0x01;
			pkt[1] = (address >> 24) & 0xff;
			pkt[2] = (address >> 16) & 0xff;
			pkt[3] = (address >> 8) & 0xff;
			pkt[4] = address & 0xff;
			await ota.writeValueWithResponse(pkt);
			logStore.addLog(`Erasing sector ${sector + 1}/${totalSectors}`);
		}
	}

	// Must match ota_bank_checksum() in Firmware/src/ble/ota_service.c: 16-bit byte sum over the whole
	// bank, where everything past the end of the image is erased flash (0xFF).
	private calculateCRC(data: Uint8Array): number {
		let crc = 0;
		for (let i = 0; i < OTA_BANK_SIZE; i++) {
			crc += i < data.length ? data[i] : 0xff;
		}
		return crc & 0xffff;
	}

	private async sendPart(
		ota: BluetoothRemoteGATTCharacteristic,
		address: number,
		data: Uint8Array
	) {
		const chunkSize = 240;
		for (let offset = 0; offset < data.length; offset += chunkSize) {
			const chunk = data.subarray(offset, offset + chunkSize);
			const pkt = new Uint8Array(1 + chunk.length);
			pkt[0] = 0x03;
			pkt.set(chunk, 1);
			await ota.writeValueWithResponse(pkt);
		}

		// Commit this page to flash
		const commitPkt = new Uint8Array(5);
		commitPkt[0] = 0x02;
		commitPkt[1] = (address >> 24) & 0xff;
		commitPkt[2] = (address >> 16) & 0xff;
		commitPkt[3] = (address >> 8) & 0xff;
		commitPkt[4] = address & 0xff;
		await ota.writeValueWithResponse(commitPkt);

		const { promise: settled, resolve } = Promise.withResolvers<void>();
		setTimeout(resolve, 50);
		await settled;
	}

	// Writes `packet` and waits for the first notification on the same characteristic accepted by
	// `accept`. Resolves to null if none arrives within timeoutMs (e.g. firmware too old to reply),
	// or if the write itself doesn't complete in that time.
	private async writeAndAwaitReply(
		characteristic: BluetoothRemoteGATTCharacteristic,
		packet: Uint8Array<ArrayBuffer>,
		accept: (reply: Uint8Array) => boolean,
		timeoutMs: number
	): Promise<Uint8Array | null> {
		const { promise: reply, resolve } = Promise.withResolvers<Uint8Array | null>();
		const onValue = (event: Event) => {
			const value = (event.target as BluetoothRemoteGATTCharacteristic).value;
			if (!value) return;
			const data = new Uint8Array(value.buffer, value.byteOffset, value.byteLength);
			if (accept(data)) resolve(data);
		};
		const timer = setTimeout(() => resolve(null), timeoutMs);
		const { promise: timedOut, resolve: timeOut } = Promise.withResolvers<'timeout'>();
		const writeTimer = setTimeout(() => timeOut('timeout'), timeoutMs);
		characteristic.addEventListener('characteristicvaluechanged', onValue);

		try {
			// The reply notification often arrives before the write's acknowledgement. Returning then
			// would leave the write in flight, and the next GATT operation fails with "GATT operation
			// already in progress", so the write always settles first.
			const written = characteristic.writeValueWithResponse(packet).then(() => 'written' as const);
			written.catch(() => undefined); // a failure after a stalled write timed out is not an error here
			if ((await Promise.race([written, timedOut])) === 'timeout') return null;
			return await reply;
		} finally {
			clearTimeout(timer);
			clearTimeout(writeTimer);
			characteristic.removeEventListener('characteristicvaluechanged', onValue);
		}
	}

	// Resolves true when the device disconnects, false if it is still connected after timeoutMs.
	private waitForDisconnect(timeoutMs: number): Promise<boolean> {
		const device = this.bleDevice;
		const { promise, resolve } = Promise.withResolvers<boolean>();
		if (!device?.gatt?.connected) {
			resolve(true);
			return promise;
		}
		const onDisconnect = () => finish(true);
		const timer = setTimeout(() => finish(false), timeoutMs);
		const finish = (disconnected: boolean) => {
			clearTimeout(timer);
			device.removeEventListener('gattserverdisconnected', onDisconnect);
			resolve(disconnected);
		};
		device.addEventListener('gattserverdisconnected', onDisconnect);
		return promise;
	}

	async flashFirmware(address: number, data: Uint8Array): Promise<void> {
		// Held for the whole update: a dropped link makes its writes fail instead of being skipped.
		const ota = this.writeCharacteristic;
		if (!ota) {
			logStore.addLog('OTA service unavailable. Is Bluetooth connected?');
			return;
		}
		if (data.length > OTA_MAX_FIRMWARE_SIZE) {
			logStore.addLog(
				`Firmware is ${data.length} bytes; the OTA bank holds at most ${OTA_MAX_FIRMWARE_SIZE}.`
			);
			return;
		}

		const startTime = Date.now();
		const pageSize = 0x100; // 256 bytes per flash page
		const crc = this.calculateCRC(data);
		const crcHex = crc.toString(16).padStart(4, '0');

		this.firmwareUploadProgress = 0;
		this.isFlashingFirmware = true;

		try {
			// Replies (CRC result, rejection) come back as notifications on this characteristic.
			await ota.startNotifications();

			await this.eraseFwArea(ota);

			logStore.addLog('Flashing firmware... wait a little.');

			let offset = 0;
			while (offset < data.length) {
				const pageData = data.subarray(offset, offset + pageSize);
				await this.sendPart(ota, address + offset, pageData);
				offset += pageData.length;
				this.firmwareUploadProgress = (offset / data.length) * 100;
			}

			logStore.addLog(
				`Firmware upload completed in ${((Date.now() - startTime) / 1000).toFixed(2)}s`
			);

			// Firmware built before April 2026 compares the CRC of command 07 with bytes 5-6 of its last
			// reply buffer instead of the command itself, so it never flashes and never answers. Stage the
			// CRC there: command 03 puts it at offset 5 of the page buffer, command 05 copies that buffer
			// into the reply buffer, and command 06 below only overwrites bytes 0-2. Harmless on newer firmware.
			await ota.writeValueWithResponse(new Uint8Array([0x03, 0, 0, 0, 0, 0, crc >> 8, crc & 0xff]));
			await this.writeAndAwaitReply(
				ota,
				new Uint8Array([0x05, 0, 0, 0, 0]),
				(d) => d.length === 20,
				3000
			);

			// Command 06: device sums the uploaded bank and replies 07 <crc hi> <crc lo>.
			logStore.addLog('Verifying flash CRC on device...');
			const verify = await this.writeAndAwaitReply(
				ota,
				new Uint8Array([0x06]),
				(d) => d.length === 3 && d[0] === 0x07,
				15000
			);
			if (verify) {
				const deviceCrc = (verify[1] << 8) | verify[2];
				if (deviceCrc !== crc) {
					logStore.addLog(
						`CRC mismatch: device has 0x${deviceCrc.toString(16).padStart(4, '0')}, expected 0x${crcHex}. Not flashing, upload again.`
					);
					return;
				}
				logStore.addLog(`Device CRC OK (0x${crcHex}).`);
			} else {
				logStore.addLog('No CRC reply from device, continuing; the device verifies it again.');
			}

			// Command 07 <magic> <crc>: the device rewrites its own flash with interrupts off and reboots,
			// so success is never acknowledged: it shows up as the link dropping. Firmware may reject with
			// 07 00 (bad command) or 07 00 <crc hi> <crc lo> (CRC mismatch); old firmware rejects silently.
			const rebooted = this.waitForDisconnect(OTA_REBOOT_TIMEOUT_MS);
			logStore.addLog('Sending final flash command: 07C001CEED' + crcHex);
			let rejected: Uint8Array | null = null;
			try {
				rejected = await this.writeAndAwaitReply(
					ota,
					new Uint8Array([0x07, 0xc0, 0x01, 0xce, 0xed, crc >> 8, crc & 0xff]),
					(d) => d[0] === 0x07 && d[1] === 0x00 && (d.length === 2 || d.length === 4),
					5000
				);
			} catch {
				// A device busy rewriting its flash stops answering; the link drop is awaited below.
			}

			if (rejected) {
				const detail =
					rejected.length === 4
						? `its CRC is 0x${((rejected[2] << 8) | rejected[3]).toString(16).padStart(4, '0')}, expected 0x${crcHex}`
						: 'bad final command';
				logStore.addLog(`Device rejected the firmware (${detail}). Nothing was flashed.`);
				return;
			}

			logStore.addLog(
				'Waiting for the device to rewrite its flash and reboot; do not remove power...'
			);
			if (await rebooted) {
				logStore.addLog(
					'Device dropped the connection: it is rewriting its flash and rebooting. Reconnect in ~10 s; the screen redraws on boot.'
				);
			} else {
				logStore.addLog(
					`Device is still connected ${OTA_REBOOT_TIMEOUT_MS / 1000} s after the final command, so it did NOT apply the update. ` +
						'Its current firmware cannot self-update over BLE: flash it once over UART (Serial firmware panel).'
				);
			}
		} catch (e) {
			await handleError(e);
		} finally {
			this.isFlashingFirmware = false;
		}
	}

	resetVariables() {
		this.gattServer = null;
		this.epdService = null;
		this.epdCharacteristic = null;
		this.rxtxCharacteristic = null;
		this.rxtxService = null;
		this.writeService = null;
		this.writeCharacteristic = null;
		this.connected = false;
		this.preconnected = false;
		this.firmwareUploadProgress = 0;
		this.imageUploadProgress = 0;
		this.isFlashingFirmware = false;
		this.isUploadingImages = false;
		this.suppressE5Notifications = false;
		this.connectedDeviceName = '';
		this.fastRefreshEnabled = false;
		this.fastRefreshSupported = false;
		this.applyDisplayModelInfo(DEFAULT_DISPLAY_INFO, 'default');
		this.selectedModel = DEFAULT_DISPLAY_INFO.model;
	}
}

export const bleConnectionStore = new BleConnectionStore();

// Helper to match your existing error handling signature.
// If you already have one elsewhere, remove this.
async function handleError(e: unknown): Promise<void> {
	console.error(e);
	logStore.addLog('Error: ' + (e instanceof Error ? e.message : String(e)));
}
