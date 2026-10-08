import { logStore } from './logStore.svelte';
import { intToHex, bytesToHex, hexToBytes } from '#lib/utils.ts';
import { buildSetTimeCommand } from '#lib/time-sync.ts';
import { FLASH_IMAGE_STORAGE_BYTES } from '#lib/photo-utils.ts';

type DisplaySource = 'default' | 'firmware' | 'manual' | 'name';

type DisplayModelInfo = {
	model: number;
	name: string;
	width: number;
	height: number;
	hasRed: boolean;
};

type StoredImageBuffers = {
	name: string;
	black: Uint8Array;
	red: Uint8Array;
};

// Heights are visible rows; the 2.13" controllers keep 128 rows per column (see canvas2bytes).
export const DISPLAY_MODEL_OPTIONS: DisplayModelInfo[] = [
	{ model: 0, name: 'Auto detect (2.9" or 2.13" BWR)', width: 250, height: 122, hasRed: true },
	{ model: 1, name: 'BW213', width: 250, height: 122, hasRed: false },
	{ model: 2, name: 'BWR213', width: 250, height: 122, hasRed: true },
	{ model: 3, name: 'BWR154', width: 200, height: 200, hasRed: true },
	{ model: 4, name: '213ICE', width: 212, height: 104, hasRed: false },
	{ model: 5, name: 'BWR290 / BWR296', width: 296, height: 128, hasRed: true },
	{ model: 6, name: 'BW290 / BW296', width: 296, height: 128, hasRed: false }
];

// Scenes of the E1 command, see SCREEN_SCENE_* in Firmware/src/application/screen.h.
export const SCENES = [
	{ id: 2, name: 'Dashboard' },
	{ id: 1, name: 'Clock' },
	{ id: 0, name: 'Image' },
	{ id: 3, name: 'Slideshow' }
] as const;

const DISPLAY_MODEL_MAP = new Map(DISPLAY_MODEL_OPTIONS.map((info) => [info.model, info]));

// Assumed until the device reports its model (E2 AB) after connecting.
const DEFAULT_DISPLAY_INFO = DISPLAY_MODEL_MAP.get(2)!;

// OTA layout, mirrors OTA_STAGING_ADDRESS/FIRMWARE_BANK_SIZE in Firmware/src/ble/ota_service.c. The
// image is always addressed at 0x20000; current firmware stores it in whichever flash bank it is not
// running from. The last 256 byte page is never written, so the largest image is one page shorter.
export const OTA_BANK_ADDRESS = 0x20000;
const OTA_BANK_SIZE = 0x20000;
export const OTA_MAX_FIRMWARE_SIZE = OTA_BANK_SIZE - 0x100;
// Firmware before v0.8.0 copies the image over itself (a few seconds) before rebooting; the link loss
// is only noticed after the supervision timeout the firmware requests (20 s), so allow more than both.
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
		'13187b10-eba9-a3ba-044e-83d3217d9a38',
		'battery_service',
		'environmental_sensing'
	];
	private rxtxServiceId = '00001f10-0000-1000-8000-00805f9b34fb';
	private rxtxCharacteristicId = '00001f1f-0000-1000-8000-00805f9b34fb';
	private edpServiceId = '13187b10-eba9-a3ba-044e-83d3217d9a38';
	private edpCharacteristicId = '4b646063-6264-f3a7-8941-e65356ea82fe';
	private writeServiceId = '0000221f-0000-1000-8000-00805f9b34fb';
	private writeCharacteristicId = '0000331f-0000-1000-8000-00805f9b34fb';

	// From picking a tag until its services are set up and the first queries are answered.
	connecting = $state(false);
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
	displayHasRed = $state(DEFAULT_DISPLAY_INFO.hasRed);
	fastRefreshEnabled = $state(false);
	fastRefreshSupported = $state(false);
	// Clock screens: minutes between new frames and whether refreshes end on the minute (E7). null
	// until the tag replies; firmware before v0.9.0 never does.
	clockIntervalMinutes: number | null = $state(null);
	clockSync: boolean | null = $state(null);
	displaySource: DisplaySource = $state('default');
	// Sensor values the tag notifies every 30 s while connected; null until the first one.
	temperatureC: number | null = $state(null);
	batteryPercent: number | null = $state(null);
	timeSyncedAt: Date | null = $state(null);
	// Reported by firmware v0.10.0 on (E1 AA, E3 AA); otherwise only known once set from this page.
	activeScene: number | null = $state(null);
	ledFlashingEnabled: boolean | null = $state(null);
	// Firmware version the tag reports (E8), e.g. "0.10.0"; null before v0.10.0, which doesn't.
	firmwareVersion: string | null = $state(null);
	// Set once the queries after connecting are done, so a missing reply means old firmware.
	firmwareVersionQueried = $state(false);

	// A transfer holds the link; other commands wait until it is done.
	get busy() {
		return this.isFlashingFirmware || this.isUploadingImages;
	}

	private applyDisplayModelInfo(info: DisplayModelInfo, source: DisplaySource) {
		this.deviceModel = info.model;
		this.deviceModelName = info.name;
		this.displayWidth = info.width;
		this.displayHeight = info.height;
		this.displayHasRed = info.hasRed;
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
		this.displayHasRed = info.hasRed;
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
			// Tags advertise "THX_" plus the MAC. Firmware up to 0.11.0 reported plain "THX" after a connection,
			// which browsers remember, so the prefix leaves out the separator.
			const device = await navigator.bluetooth.requestDevice({
				filters: [{ namePrefix: 'THX' }],
				optionalServices: this.bleDeviceOptionalServicesIds
			});
			this.bleDevice?.removeEventListener('gattserverdisconnected', this.onGattDisconnected);
			device.addEventListener('gattserverdisconnected', this.onGattDisconnected);
			this.bleDevice = device;

			this.connectedDeviceName = device.name ?? 'Unknown device';

			const inferredDisplay = inferDisplayModelFromName(device.name);
			if (inferredDisplay) {
				this.applyDisplayModelInfo(inferredDisplay, 'name');
			}

			this.connecting = true;
			await this.connect();
		} catch (e) {
			// A half-finished connection (e.g. a device without these services) must not stay open.
			if (this.bleDevice?.gatt?.connected) {
				this.bleDevice.gatt.disconnect();
			}
			this.resetVariables();
			await handleError(e);
		} finally {
			this.connecting = false;
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

			if (data.byteLength === 3 && data[0] === 0xe7) {
				this.clockIntervalMinutes = data[1];
				this.clockSync = data[2] === 0x01;
				logStore.addLog(
					`[From display][RXTX]: Clock frame every ${data[1]} min${this.clockSync ? ', ending on the minute' : ''}`
				);
				return;
			}

			if (data.byteLength === 3 && data[0] === 0xe1 && data[1] === 0xaa) {
				this.activeScene = data[2];
				return;
			}

			if (data.byteLength === 3 && data[0] === 0xe3 && data[1] === 0xaa) {
				this.ledFlashingEnabled = data[2] === 0x01;
				return;
			}

			if (data.byteLength >= 3 && data[0] === 0xe8) {
				this.firmwareVersion = new TextDecoder().decode(data.subarray(1));
				logStore.addLog(`[From display][RXTX]: Firmware ${this.firmwareVersion}`);
				return;
			}

			const hex = bytesToHex(data);

			// E2 AA reply: int16 LE, tenths of a degree.
			if (value.byteLength === 2) {
				this.temperatureC = value.getInt16(0, true) / 10;
				logStore.addLog(`[From display][RXTX]: Temperature ${this.temperatureC.toFixed(1)}°C`);
				return;
			}

			// Fallback: log raw payload
			logStore.addLog(`[From display][RXTX]: ${hex}`);
		});

		this.connected = true;

		// Allow BLE connection parameters and CCCD writes to stabilise
		// before querying the device, otherwise the firmware may silently
		// drop the notification response.
		const { promise: settled, resolve } = Promise.withResolvers<void>();
		setTimeout(resolve, 600);
		await settled;
		await this.queryDisplayInfo();
		await this.syncTime();
		// One more round trip after E8: a version reply would have arrived by now.
		this.firmwareVersionQueried = true;
		await this.subscribeSensors();
	}

	// Battery level and temperature, read once and then notified by the tag every 30 s. Optional:
	// a tag without these services still works.
	private async subscribeSensors() {
		const sensors = [
			{
				service: 'battery_service',
				characteristic: 'battery_level',
				apply: (value: DataView) => (this.batteryPercent = value.getUint8(0))
			},
			{
				service: 'environmental_sensing',
				characteristic: 0x2a1f, // temperature in tenths of a degree C
				apply: (value: DataView) => (this.temperatureC = value.getInt16(0, true) / 10)
			}
		];
		for (const sensor of sensors) {
			try {
				const service = await this.gattServer!.getPrimaryService(sensor.service);
				const characteristic = await service.getCharacteristic(sensor.characteristic);
				characteristic.addEventListener('characteristicvaluechanged', (event: Event) => {
					const value = (event.target as BluetoothRemoteGATTCharacteristic).value;
					if (value) sensor.apply(value);
				});
				sensor.apply(await characteristic.readValue());
				await characteristic.startNotifications();
			} catch (e) {
				logStore.addLog(
					`No ${sensor.service.replace('_', ' ')} on this tag (${e instanceof Error ? e.message : e}).`
				);
			}
		}
	}

	// Sets the tag's clock and time zone from this browser. Sent on every connect; the firmware
	// also uses the interval between syncs to correct its clock drift.
	async syncTime() {
		if (!this.rxtxCharacteristic) {
			logStore.addLog('Service unavailable. Is Bluetooth connected?');
			return;
		}
		const command = buildSetTimeCommand(new Date());
		logStore.addLog(`Setting the time: ${new Date().toLocaleString()}`);
		try {
			await this.rxtxCharacteristic.writeValueWithResponse(command);
			this.timeSyncedAt = new Date();
		} catch (e) {
			await handleError(e);
		}
	}

	async queryDisplayInfo() {
		if (!this.rxtxCharacteristic) {
			logStore.addLog('Service unavailable. Is Bluetooth connected?');
			return;
		}

		logStore.addLog('Querying display model...');
		await this.sendRxTxCommand('e2ab');
		await this.queryFastRefreshInfo();
		// Firmware before v0.9.0 ignores E7, before v0.10.0 also E1 AA, E3 AA and E8.
		await this.sendRxTxCommand('e7aa');
		await this.sendRxTxCommand('e1aa');
		await this.sendRxTxCommand('e3aa');
		await this.sendRxTxCommand('e8');
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

	// Every 1 to 60 minutes; sync starts each refresh early so the new time shows as the minute changes.
	async setClockSchedule(minutes: number, sync: boolean) {
		this.clockIntervalMinutes = minutes;
		this.clockSync = sync;
		await this.sendRxTxCommand(`e7${intToHex(minutes, 1)}${sync ? '01' : '00'}`);
	}

	async setDisplayModel(model: number) {
		this.selectedModel = model;
		if (model !== 0) {
			this.applyDisplayModelInfo(resolveDisplayModel(model), 'manual');
		}

		await this.sendRxTxCommand(`e0${intToHex(model, 1)}`);
		await this.queryDisplayInfo();
	}

	async setScene(scene: number) {
		if (await this.sendRxTxCommand(`e1${intToHex(scene, 1)}`)) this.activeScene = scene;
	}

	async setLedFlashing(enabled: boolean) {
		if (await this.sendRxTxCommand(enabled ? 'e301' : 'e300')) this.ledFlashingEnabled = enabled;
	}

	async playLedRainbow(play: boolean) {
		await this.sendRxTxCommand(play ? 'e401' : 'e400');
	}

	// Redraws the current screen with a full refresh.
	async redraw() {
		await this.sendRxTxCommand('e200');
	}

	async requestTemperature() {
		await this.sendRxTxCommand('e2aa');
	}

	// Fills the screen with a repeating byte (a test pattern).
	async drawPattern(byte: number) {
		await this.sendRxTxCommand(`b1${intToHex(byte & 0xff, 1)}`);
	}

	// Errors are logged, not thrown: the UI buttons call this without handling failures. Returns
	// whether the write went through.
	private async sendRxTxCommand(command: string): Promise<boolean> {
		if (!this.rxtxCharacteristic) {
			logStore.addLog('Service unavailable. Is Bluetooth connected?');
			return false;
		}
		logStore.addLog(`Sending RXTX command: ${command}`);
		try {
			await this.rxtxCharacteristic.writeValueWithResponse(hexToBytes(command) as BufferSource);
			return true;
		} catch (e) {
			await handleError(e);
			return false;
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

			// E5 00: the device makes room for the images and replies E5 00 <ok>. Firmware before
			// v0.13.0 erases the whole image flash here (a few seconds); later firmware erases each
			// sector when the first chunk reaches it.
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

						const chunk = buffer.subarray(offset, offset + chunkSize);
						// Erased flash reads 0xFF, so all-white stretches of the black plane need no write:
						// every firmware erases what the upload covers, at the latest on E5 02.
						if (!chunk.every((byte) => byte === 0xff)) {
							const packet = new Uint8Array(6 + chunk.length);

							packet[0] = 0xe5;
							packet[1] = 0x01;
							packet[2] = index;
							packet[3] = plane;
							packet[4] = offset & 0xff;
							packet[5] = (offset >> 8) & 0xff;
							packet.set(chunk, 6);

							await rxtx.writeValueWithResponse(packet);
						}
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

			// E5 02: the device erases the sectors no chunk reached, commits the upload, replies
			// E5 02 <ok> and starts showing it.
			const committed = await this.writeAndAwaitReply(
				rxtx,
				new Uint8Array([0xe5, 0x02]),
				isReplyTo(0x02),
				15000
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

		// Commit this page to flash; the tag writes it before it acknowledges the write.
		const commitPkt = new Uint8Array(5);
		commitPkt[0] = 0x02;
		commitPkt[1] = (address >> 24) & 0xff;
		commitPkt[2] = (address >> 16) & 0xff;
		commitPkt[3] = (address >> 8) & 0xff;
		commitPkt[4] = address & 0xff;
		await ota.writeValueWithResponse(commitPkt);
	}

	// Firmware v0.11.0 on: command 08 <bank offset:3> <data> puts data at a bank offset in one write,
	// and the tag erases each sector when the upload reaches it, so there is no erase pass and no
	// separate commit per page. Every write waits for its response: written without response, the
	// writes piled up while the tag erased a sector and it stopped answering after about 10 KiB.
	// 240-byte writes are what the page-by-page upload has always sent.
	private async writeFirmwareAtOffsets(ota: BluetoothRemoteGATTCharacteristic, data: Uint8Array) {
		const chunkSize = 236;
		for (let offset = 0; offset < data.length; offset += chunkSize) {
			const chunk = data.subarray(offset, offset + chunkSize);
			const packet = new Uint8Array(4 + chunk.length);
			packet[0] = 0x08;
			packet[1] = (offset >> 16) & 0xff;
			packet[2] = (offset >> 8) & 0xff;
			packet[3] = offset & 0xff;
			packet.set(chunk, 4);
			await ota.writeValueWithResponse(packet);
			this.firmwareUploadProgress = ((offset + chunk.length) / data.length) * 100;
		}
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

			// Firmware v0.11.0 on marks command 08 support by also accepting writes without response.
			if (ota.properties.writeWithoutResponse) {
				logStore.addLog('Writing firmware...');
				await this.writeFirmwareAtOffsets(ota, data);
			} else {
				await this.eraseFwArea(ota);
				logStore.addLog('Flashing firmware page by page (firmware before v0.11.0)...');
				let offset = 0;
				while (offset < data.length) {
					const pageData = data.subarray(offset, offset + pageSize);
					await this.sendPart(ota, address + offset, pageData);
					offset += pageData.length;
					this.firmwareUploadProgress = (offset / data.length) * 100;
				}

				// Firmware built before April 2026 compares the CRC of command 07 with bytes 5-6 of its
				// last reply buffer instead of the command itself, so it never flashes and never answers.
				// Stage the CRC there: command 03 puts it at offset 5 of the page buffer, command 05
				// copies that buffer into the reply buffer, and command 06 below only overwrites bytes 0-2.
				await ota.writeValueWithResponse(
					new Uint8Array([0x03, 0, 0, 0, 0, 0, crc >> 8, crc & 0xff])
				);
				await this.writeAndAwaitReply(
					ota,
					new Uint8Array([0x05, 0, 0, 0, 0]),
					(d) => d.length === 20,
					3000
				);
			}

			logStore.addLog(
				`Firmware upload completed in ${((Date.now() - startTime) / 1000).toFixed(2)}s`
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

			// Command 07 <magic> <crc>: the device starts the new firmware (firmware before v0.8.0 first
			// copies it over itself with interrupts off), so success is never acknowledged: it shows up as
			// the link dropping. Firmware may reject with 07 00 (bad command, or not a bootable image) or
			// 07 00 <crc hi> <crc lo> (CRC mismatch); old firmware rejects silently.
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
						: 'bad final command, or not a bootable image';
				logStore.addLog(`Device rejected the firmware (${detail}). Nothing was flashed.`);
				return;
			}

			logStore.addLog(
				'Waiting for the device to reboot into the new firmware; do not remove power...'
			);
			if (await rebooted) {
				logStore.addLog(
					'Device dropped the connection: it is rebooting into the new firmware. Reconnect in ~10 s; the screen redraws on boot.'
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
		this.connecting = false;
		this.firmwareUploadProgress = 0;
		this.imageUploadProgress = 0;
		this.isFlashingFirmware = false;
		this.isUploadingImages = false;
		this.suppressE5Notifications = false;
		this.connectedDeviceName = '';
		this.fastRefreshEnabled = false;
		this.fastRefreshSupported = false;
		this.clockIntervalMinutes = null;
		this.clockSync = null;
		this.applyDisplayModelInfo(DEFAULT_DISPLAY_INFO, 'default');
		this.selectedModel = DEFAULT_DISPLAY_INFO.model;
		this.temperatureC = null;
		this.batteryPercent = null;
		this.timeSyncedAt = null;
		this.activeScene = null;
		this.ledFlashingEnabled = null;
		this.firmwareVersion = null;
		this.firmwareVersionQueried = false;
	}
}

export const bleConnectionStore = new BleConnectionStore();

// Logs a failed BLE operation; the UI calls the store without handling errors itself.
async function handleError(e: unknown): Promise<void> {
	console.error(e);
	logStore.addLog('Error: ' + (e instanceof Error ? e.message : String(e)));
}
