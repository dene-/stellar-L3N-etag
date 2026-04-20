// Declare globals that may be provided elsewhere
import { logStore } from './logStore.svelte';
import { intToHex, canvas2bytes, bytesToHex, decimalToHex, hexToBytes } from '$lib/utils';

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
const FLASH_SLIDESHOW_CAPACITY_BYTES = 0x37000;

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
	isFlashingFirmware = $state(false);
	connectedDeviceName = $state('');
	deviceModel = $state(DEFAULT_DISPLAY_INFO.model);
	deviceModelName = $state(DEFAULT_DISPLAY_INFO.name);
	displayWidth = $state(DEFAULT_DISPLAY_INFO.width);
	displayHeight = $state(DEFAULT_DISPLAY_INFO.height);
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

	// Send a hex string buffer in chunks to the EPD characteristic in either BW or BWR mode
	async sendBufferData(valueHex: string, type: 'bw' | 'bwr') {
		if (!this.epdCharacteristic) {
			logStore.addLog('Service unavailable. Is Bluetooth connected?');
			return;
		}

		const code = type === 'bwr' ? '00' : 'ff';
		const step = 480; // hex chars per chunk (240 bytes)
		let partIndex = 0;
		for (let i = 0; i < valueHex.length; i += step) {
			logStore.addLog(
				`Sending block ${partIndex + 1}. Size: ${step / 2 + 4} bytes. Offset: ${i / 2}`
			);
			const chunk = valueHex.substring(i, i + step);
			const pkt = '03' + code + intToHex(i / 2, 2) + chunk;
			await this.sendEpdCommand(pkt);
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
		await this.sendBufferData(bytesToHex(new Uint8Array(bw).buffer), 'bw');
		await this.sendBufferData(bytesToHex(new Uint8Array(bwr).buffer), 'bwr');

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

		if (totalBytes > FLASH_SLIDESHOW_CAPACITY_BYTES) {
			throw new Error('Selected images exceed the MCU flash space reserved for photos.');
		}

		for (const image of images) {
			if (image.black.length !== planeSize || image.red.length !== planeSize) {
				throw new Error('All rendered images must share the same display size.');
			}
		}

		this.isFlashingFirmware = true;
		let chunkErrors = 0;

		// Temporarily listen for E5 error notifications during upload
		const errorListener = (event: Event) => {
			const char = event.target as BluetoothRemoteGATTCharacteristic;
			const v = char.value;
			if (!v || v.byteLength < 3) return;
			const d = new Uint8Array(v.buffer, v.byteOffset, v.byteLength);
			if (d[0] === 0xe5 && d[2] === 0x00) {
				chunkErrors++;
			}
		};
		this.rxtxCharacteristic.addEventListener('characteristicvaluechanged', errorListener);

		try {
			const start = Date.now();
			logStore.addLog(`Preparing persistent upload for ${images.length} image(s)...`);
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

			// Wait for flash erase and BLE connection speed change
			await new Promise((r) => setTimeout(r, 500));

			const totalChunks = images.length * 2 * Math.ceil(planeSize / chunkSize);
			let chunksDone = 0;

			for (const [index, image] of images.entries()) {
				for (const [plane, buffer] of [image.black, image.red].entries()) {
					for (let offset = 0; offset < buffer.length; offset += chunkSize) {
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
					}
				}
				logStore.addLog(
					`Image ${index + 1}/${images.length} sent (${Math.round((chunksDone / totalChunks) * 100)}%)`
				);
			}

			await this.rxtxCharacteristic.writeValueWithResponse(new Uint8Array([0xe5, 0x02]));
			const elapsed = ((Date.now() - start) / 1000).toFixed(1);

			if (chunkErrors > 0) {
				logStore.addLog(
					`Upload finished with ${chunkErrors} chunk error(s) in ${elapsed}s. Display may not update correctly.`
				);
			} else {
				logStore.addLog(
					images.length > 1
						? `Slideshow uploaded in ${elapsed}s. Interval: ${slideshowInterval}s`
						: `Photo uploaded in ${elapsed}s (persistent single-photo mode).`
				);
			}
		} finally {
			this.rxtxCharacteristic.removeEventListener('characteristicvaluechanged', errorListener);
			this.isFlashingFirmware = false;
		}
	}

	private async eraseFwArea() {
		const fwAreaSize = 0x20000;
		let fwCurAddress = 0x20000;
		while (fwCurAddress < 0x20000 + fwAreaSize) {
			const hex_address = decimalToHex(fwCurAddress, 8);

			await this.writeCharacteristic?.writeValue(hexToBytes('01' + hex_address) as BufferSource);

			fwCurAddress += 0x1000;
		}
	}

	private calculateCRC(localData: string) {
		let checkPosistion = 0;
		let outCRC = 0;

		while (checkPosistion < 0x40000) {
			if (checkPosistion < localData.length)
				outCRC += Number('0x' + localData.substring(checkPosistion, checkPosistion + 2));
			else outCRC += 0xff;
			checkPosistion += 2;
		}

		return decimalToHex(outCRC & 0xffff, 4);
	}

	private async sendPart(address: number, data: string) {
		const hex_address = decimalToHex(address, 8);
		const part_len = 480;

		while (data.length) {
			let cur_part_len = part_len;

			if (data.length < part_len) {
				cur_part_len = data.length;
			}

			const data_part = data.substring(0, cur_part_len);
			data = data.substring(cur_part_len);

			await this.writeCharacteristic?.writeValue(hexToBytes('03' + data_part) as BufferSource);
		}
		await this.writeCharacteristic?.writeValue(hexToBytes('02' + hex_address) as BufferSource);

		await new Promise((resolve) => setTimeout(resolve, 50));
	}

	async flashFirmware(address: number, data: string): Promise<void> {
		const startTime = new Date().getTime();
		const part_len = 0x200;
		const inCRC = this.calculateCRC(data);
		const totalDataLength = data.length;
		let addressOffset = 0;

		this.firmwareUploadProgress = 0;
		this.isFlashingFirmware = true;

		await this.eraseFwArea();

		logStore.addLog('Flashing firmware... wait a little.');

		while (data.length) {
			let cur_part_len = part_len;

			if (data.length < part_len) {
				cur_part_len = data.length;
			}

			const data_part = data.substring(0, cur_part_len);

			data = data.substring(cur_part_len);
			await this.sendPart(address + addressOffset, data_part);
			addressOffset += cur_part_len / 2;

			this.firmwareUploadProgress = (addressOffset / (totalDataLength / 2)) * 100;
		}

		logStore.addLog('Sending final flash: ' + '07C001CEED' + inCRC);
		logStore.addLog(
			`Firmware flashed completed in ${((Date.now() - startTime) / 1000).toFixed(2)}s`
		);
		this.isFlashingFirmware = false;
		await this.writeCharacteristic?.writeValue(hexToBytes('07C001CEED' + inCRC) as BufferSource);
	}

	resetVariables() {
		this.gattServer = null;
		this.epdService = null;
		this.epdCharacteristic = null;
		this.rxtxCharacteristic = null;
		this.rxtxService = null;
		this.connected = false;
		this.preconnected = false;
		this.firmwareUploadProgress = 0;
		this.isFlashingFirmware = false;
		this.connectedDeviceName = '';
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
