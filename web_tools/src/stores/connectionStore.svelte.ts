// Declare globals that may be provided elsewhere
import { logStore } from './logStore.svelte';
import { intToHex, canvas2bytes, bytesToHex, hexToBytes } from '$lib/utils';
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
	{ model: 0, name: 'Auto detect', width: 250, height: 128 },
	{ model: 1, name: 'BW213', width: 250, height: 128 },
	{ model: 2, name: 'BWR213', width: 250, height: 128 },
	{ model: 3, name: 'BWR154', width: 200, height: 200 },
	{ model: 4, name: '213ICE', width: 212, height: 104 },
	{ model: 5, name: 'BWR290 / BWR296', width: 296, height: 128 }
];

const DISPLAY_MODEL_MAP = new Map(DISPLAY_MODEL_OPTIONS.map((info) => [info.model, info]));
const DEFAULT_DISPLAY_INFO = DISPLAY_MODEL_MAP.get(2)!;

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
	deviceModel = $state(DEFAULT_DISPLAY_INFO.model);
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

	disconnect() {
		this.resetVariables();

		logStore.addLog('Disconnected.');

		//document.getElementById('connectbutton').innerHTML = 'Connect';
	}

	async preConnect() {
		if (this.gattServer != null && this.gattServer.connected) {
			if (this.bleDevice && this.bleDevice?.gatt?.connected) {
				this.bleDevice.gatt.disconnect();
			}
		} else {
			this.bleDevice = await navigator.bluetooth.requestDevice({
				optionalServices: this.bleDeviceOptionalServicesIds,
				acceptAllDevices: true
			});

			this.connectedDeviceName = this.bleDevice.name ?? 'Unknown device';

			const inferredDisplay = inferDisplayModelFromName(this.bleDevice.name);
			if (inferredDisplay) {
				this.applyDisplayModelInfo(inferredDisplay, 'name');
			}

			// Ensure correct "this" binding on disconnect
			this.bleDevice.addEventListener('gattserverdisconnected', this.disconnect.bind(this));

			this.preconnected = true;

			try {
				await this.connect();
			} catch (e) {
				await handleError(e);
			}
		}
	}

	async reConnect() {
		if (this.bleDevice != null && this.bleDevice?.gatt?.connected) {
			this.bleDevice.gatt.disconnect();
		}

		this.resetVariables();

		logStore.addLog('Reconnecting...');

		setTimeout(async () => {
			await this.connect();
		}, 300);
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

			if (data.byteLength === 7 && data[0] === 0xe2 && data[1] === 0xab) {
				const model = data[2];
				const width = data[3] | (data[4] << 8);
				const height = data[5] | (data[6] << 8);

				this.applyDisplayGeometry(model, width, height, 'firmware');
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
		if (model !== 0) {
			this.applyDisplayModelInfo(resolveDisplayModel(model), 'manual');
		}

		await this.sendRxTxCommand(`e0${intToHex(model, 1)}`);
		await this.queryDisplayInfo();
	}

	async sendRxTxCommand(command: string) {
		if (this.rxtxCharacteristic) {
			logStore.addLog(`Sending RXTX command: ${command}`);
			await this.rxtxCharacteristic.writeValueWithResponse(hexToBytes(command) as BufferSource);
		} else {
			logStore.addLog('Service unavailable. Is Bluetooth connected?');
		}
	}

	async sendEpdCommand(command: string) {
		if (this.epdCharacteristic) {
			logStore.addLog(`Sending EPD command: ${command}`);
			await this.epdCharacteristic.writeValueWithResponse(hexToBytes(command) as BufferSource);
		} else {
			logStore.addLog('Service unavailable. Is Bluetooth connected?');
		}
	}

	// Send a Uint8Array buffer in chunks to the EPD characteristic in either BW or BWR mode
	async sendBufferData(data: Uint8Array, type: 'bw' | 'bwr') {
		if (!this.epdCharacteristic) {
			logStore.addLog('Service unavailable. Is Bluetooth connected?');
			return;
		}

		const code = type === 'bwr' ? 0x00 : 0xff;
		const chunkSize = 240; // bytes per chunk
		let partIndex = 0;
		for (let offset = 0; offset < data.length; offset += chunkSize) {
			logStore.addLog(
				`Sending block ${partIndex + 1}. Size: ${Math.min(chunkSize, data.length - offset) + 4} bytes. Offset: ${offset}`
			);
			const chunk = data.subarray(offset, offset + chunkSize);
			const pkt = new Uint8Array(4 + chunk.length);
			pkt[0] = 0x03;
			pkt[1] = code;
			pkt[2] = (offset >> 8) & 0xff;
			pkt[3] = offset & 0xff;
			pkt.set(chunk, 4);
			await this.epdCharacteristic.writeValueWithResponse(pkt);
			partIndex += 1;
		}
	}

	// Prepare display and upload both BW and BWR buffers from a canvas, then trigger full refresh
	async uploadImageFromCanvas(canvas: HTMLCanvasElement) {
		const start = Date.now();
		await this.sendEpdCommand('0000');
		await this.sendEpdCommand('020000');

		const bw = canvas2bytes(canvas, 'bw');
		const bwr = canvas2bytes(canvas, 'bwr');
		await this.sendBufferData(bw, 'bw');
		await this.sendBufferData(bwr, 'bwr');

		await this.sendEpdCommand('0101');
		logStore.addLog(`Refresh done, took ${((Date.now() - start) / 1000).toFixed(2)}s`);
	}

	async uploadImageSet(images: StoredImageBuffers[], intervalSeconds: number): Promise<void> {
		if (!this.rxtxCharacteristic) {
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
		const slideshowInterval = images.length > 1 ? intervalSeconds : 0;

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

		// Listen for E5 notifications during upload
		const notificationListener = (event: Event) => {
			const char = event.target as BluetoothRemoteGATTCharacteristic;
			const v = char.value;
			if (!v || v.byteLength < 3) return;
			const d = new Uint8Array(v.buffer, v.byteOffset, v.byteLength);
			if (d[0] !== 0xe5) return;

			// E5 xx 00 = failure
			if (d[2] === 0x00) {
				const subcmd = d[1];
				logStore.addLog(`Device rejected E5 sub-command 0x${subcmd.toString(16).padStart(2, '0')}`);
				uploadAborted = true;
			}
		};
		this.rxtxCharacteristic.addEventListener('characteristicvaluechanged', notificationListener);

		try {
			const start = Date.now();
			logStore.addLog(`Preparing persistent upload for ${images.length} image(s)...`);

			// Send E5 00 prepare command
			await this.rxtxCharacteristic.writeValueWithResponse(
				new Uint8Array([
					0xe5,
					0x00,
					uploadModel,
					images.length,
					slideshowInterval & 0xff,
					(slideshowInterval >> 8) & 0xff
				])
			);

			// Wait for flash erase and BLE connection speed change,
			// also allows the prepare response notification to arrive
			await new Promise((r) => setTimeout(r, 500));

			if (uploadAborted) {
				logStore.addLog('Device rejected the prepare command. Upload aborted.');
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

						await this.rxtxCharacteristic.writeValueWithResponse(packet);
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

			await this.rxtxCharacteristic.writeValueWithResponse(new Uint8Array([0xe5, 0x02]));
			const elapsed = ((Date.now() - start) / 1000).toFixed(1);

			this.imageUploadProgress = 100;
			logStore.addLog(
				images.length > 1
					? `Slideshow uploaded in ${elapsed}s. Interval: ${slideshowInterval}s`
					: `Photo uploaded in ${elapsed}s (persistent single-photo mode).`
			);
		} finally {
			this.rxtxCharacteristic.removeEventListener(
				'characteristicvaluechanged',
				notificationListener
			);
			this.isFlashingFirmware = false;
			this.isUploadingImages = false;
			this.suppressE5Notifications = false;
		}
	}

	private async eraseFwArea() {
		const fwAreaSize = 0x20000;
		let fwCurAddress = 0x20000;
		const totalSectors = fwAreaSize / 0x1000;
		let sectorsDone = 0;
		while (fwCurAddress < 0x20000 + fwAreaSize) {
			const pkt = new Uint8Array(5);
			pkt[0] = 0x01;
			pkt[1] = (fwCurAddress >> 24) & 0xff;
			pkt[2] = (fwCurAddress >> 16) & 0xff;
			pkt[3] = (fwCurAddress >> 8) & 0xff;
			pkt[4] = fwCurAddress & 0xff;
			await this.writeCharacteristic?.writeValue(pkt);
			fwCurAddress += 0x1000;
			sectorsDone++;
			logStore.addLog(`Erasing sector ${sectorsDone}/${totalSectors}`);
		}
	}

	private calculateCRC(data: Uint8Array): number {
		let crc = 0;
		const totalSize = 0x20000; // OTA area size
		for (let i = 0; i < totalSize; i++) {
			crc += i < data.length ? data[i] : 0xff;
		}
		return crc & 0xffff;
	}

	private async sendPart(address: number, data: Uint8Array) {
		const chunkSize = 240;
		for (let offset = 0; offset < data.length; offset += chunkSize) {
			const chunk = data.subarray(offset, offset + chunkSize);
			const pkt = new Uint8Array(1 + chunk.length);
			pkt[0] = 0x03;
			pkt.set(chunk, 1);
			await this.writeCharacteristic?.writeValue(pkt);
		}

		// Commit this page to flash
		const commitPkt = new Uint8Array(5);
		commitPkt[0] = 0x02;
		commitPkt[1] = (address >> 24) & 0xff;
		commitPkt[2] = (address >> 16) & 0xff;
		commitPkt[3] = (address >> 8) & 0xff;
		commitPkt[4] = address & 0xff;
		await this.writeCharacteristic?.writeValue(commitPkt);

		await new Promise((resolve) => setTimeout(resolve, 50));
	}

	async flashFirmware(address: number, data: Uint8Array): Promise<void> {
		const startTime = Date.now();
		const pageSize = 0x100; // 256 bytes per flash page
		const crc = this.calculateCRC(data);
		const crcHex = crc.toString(16).padStart(4, '0');

		this.firmwareUploadProgress = 0;
		this.isFlashingFirmware = true;

		await this.eraseFwArea();

		logStore.addLog('Flashing firmware... wait a little.');

		let offset = 0;
		while (offset < data.length) {
			const pageData = data.subarray(offset, offset + pageSize);
			await this.sendPart(address + offset, pageData);
			offset += pageData.length;
			this.firmwareUploadProgress = (offset / data.length) * 100;
		}

		logStore.addLog(
			`Firmware upload completed in ${((Date.now() - startTime) / 1000).toFixed(2)}s`
		);

		// Send case 6 (on-device CRC verification) before case 7.
		// This sets crc_verified on older firmware that requires it.
		logStore.addLog('Verifying flash CRC on device...');
		await this.writeCharacteristic?.writeValue(new Uint8Array([0x06]) as BufferSource);
		// Wait for the device to read back 128KB of flash and compute CRC
		await new Promise((resolve) => setTimeout(resolve, 3000));

		logStore.addLog('Sending final flash command: 07C001CEED' + crcHex);
		await this.writeCharacteristic?.writeValue(hexToBytes('07C001CEED' + crcHex) as BufferSource);

		logStore.addLog('Flash command sent — device should reboot now.');

		this.isFlashingFirmware = false;
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
	}
}

export const bleConnectionStore = new BleConnectionStore();

// Helper to match your existing error handling signature.
// If you already have one elsewhere, remove this.
async function handleError(e: unknown): Promise<void> {
	console.error(e);
	logStore.addLog('Error: ' + (e instanceof Error ? e.message : String(e)));
}
