import { TlsrSerialFlasher } from '#lib/tlsr-serial-flasher.ts';
import { logStore } from './logStore.svelte';

const IDLE_STATUS = 'Open the serial port of your USB adapter to start.';

function errorText(error: unknown) {
	return error instanceof Error ? error.message : String(error);
}

// The serial flasher and what the Firmware page shows of it. It lives here, not in the card, so
// leaving the page never closes the port or drops a write in progress.
class SerialFlashStore {
	readonly supported = TlsrSerialFlasher.isSupported();
	connected = $state(false);
	busy = $state(false);
	progress = $state(0);
	status = $state(IDLE_STATUS);
	baudRate = $state('460800');
	activationMs = $state('3000');

	private readonly flasher = new TlsrSerialFlasher({
		log: (message: string) => logStore.addLog(message),
		onStatus: (message: string) => (this.status = message),
		onProgress: (percent: number) => (this.progress = percent),
		onConnectionChange: (open: boolean) => {
			this.connected = open;
			if (!open) {
				this.progress = 0;
				this.status = IDLE_STATUS;
			}
		}
	});

	async togglePort(hasFile: boolean) {
		if (this.busy) return;
		const closing = this.connected;
		try {
			if (closing) {
				await this.flasher.close();
				return;
			}
			await this.flasher.open(Number(this.baudRate));
			this.status = hasFile
				? 'Ready. Unlock the flash, then write the firmware.'
				: 'Choose a firmware file.';
		} catch (error) {
			logStore.addLog('Serial port: ' + errorText(error));
			this.status = closing ? IDLE_STATUS : 'Could not open the serial port.';
		}
	}

	unlockFlash() {
		return this.run(() => this.flasher.unlockFlash(Number(this.activationMs)));
	}

	writeFirmware(data: Uint8Array) {
		return this.run(() => this.flasher.flashFirmware(data, Number(this.activationMs)));
	}

	eraseAllFlash() {
		return this.run(() => this.flasher.eraseAllFlash(Number(this.activationMs)));
	}

	resetChip() {
		return this.run(() => this.flasher.softResetWithActivation(Number(this.activationMs)));
	}

	// busy is set before the first await, so a second click can't start a second operation.
	private async run(action: () => Promise<void>) {
		if (this.busy) return;
		this.busy = true;
		try {
			await action();
		} catch (error) {
			logStore.addLog('Serial flashing: ' + errorText(error));
			this.status = 'Flashing failed; see the activity log.';
		} finally {
			this.busy = false;
		}
	}
}

export const serialFlashStore = new SerialFlashStore();
