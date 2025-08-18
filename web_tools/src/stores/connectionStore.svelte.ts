// Declare globals that may be provided elsewhere
import { logStore } from "./logStore.svelte";

function hexToBytes(hex: string): Uint8Array {
  const bytes: number[] = [];
  for (let c = 0; c < hex.length; c += 2) {
    bytes.push(parseInt(hex.slice(c, c + 2), 16));
  }
  return new Uint8Array(bytes);
}

function bytesToHex(data: ArrayBuffer | ArrayLike<number> | ArrayBufferLike): string {
  return new Uint8Array(data as ArrayBuffer).reduce((memo, i) => {
    return memo + ("0" + i.toString(16)).slice(-2);
  }, "");
}

function intToHex(intIn: number, bytes: number = 4): string {
  return intIn.toString(16).padStart(bytes * 2, "0");
}

class BleConnectionStore {
  private bleDevice: BluetoothDevice | null = $state(null);
  private gattServer: BluetoothRemoteGATTServer | null = $state(null);
  private rxtxService: BluetoothRemoteGATTService | null = $state(null);
  private rxtxCharacteristic: BluetoothRemoteGATTCharacteristic | null = $state(null);
  private epdService: BluetoothRemoteGATTService | null = $state(null);
  private epdCharacteristic: BluetoothRemoteGATTCharacteristic | null = $state(null);

  private bleDeviceOptionalServicesIds: string[] = [
    '0000221f-0000-1000-8000-00805f9b34fb',
    '00001f10-0000-1000-8000-00805f9b34fb',
    '13187b10-eba9-a3ba-044e-83d3217d9a38',
  ];
  private rxtxServiceId = '00001f10-0000-1000-8000-00805f9b34fb';
  private rxtxCharacteristicId = '00001f1f-0000-1000-8000-00805f9b34fb';
  private epdServiceId = '13187b10-eba9-a3ba-044e-83d3217d9a38';
  private epdCharacteristicId = '4b646063-6264-f3a7-8941-e65356ea82fe';

  preconnected = $state(false);
  connected = $state(false);

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

    logStore.addLog('Reconnecting');

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
    logStore.addLog('Found GATT server');

    this.epdService = await this.gattServer.getPrimaryService(this.epdServiceId);
    logStore.addLog('Found service');

    this.epdCharacteristic = await this.epdService.getCharacteristic(this.epdCharacteristicId);
    logStore.addLog('Service connected');

    await this.epdCharacteristic.startNotifications();

    this.epdCharacteristic.addEventListener('characteristicvaluechanged', (event: Event) => {
      const characteristic = event.target as BluetoothRemoteGATTCharacteristic;
      console.log(characteristic);
      const value = characteristic.value;

      if (!value) {
        logStore.addLog('[From display]: No data');
        return;
      }

      const hex = bytesToHex(value.buffer);
      console.log('epd ret', hex);

      const count = parseInt('0x' + hex);

      logStore.addLog(`[From display]: Received ${count} bytes`);
    });

    // document.getElementById('connectbutton').innerHTML = 'Disconnect';
    await this.connectRXTX();
  }

  async connectRXTX() {
    if (!this.gattServer) {
      throw new Error('No GATT server available.');
    }

    this.rxtxService = await this.gattServer.getPrimaryService(this.rxtxServiceId);
    logStore.addLog('Found UART service');

    this.rxtxCharacteristic = await this.rxtxService.getCharacteristic(
      this.rxtxCharacteristicId
    );

    // Start notifications to receive async responses from the device (e.g. E2 AA temperature)
    await this.rxtxCharacteristic.startNotifications();

    this.rxtxCharacteristic.addEventListener('characteristicvaluechanged', (event: Event) => {
      const characteristic = event.target as BluetoothRemoteGATTCharacteristic;
      console.log(characteristic);
      const value = characteristic.value;

      if (!value) {
        logStore.addLog('[From display]: No data');
        return;
      }

      const hex = bytesToHex(value.buffer);
      console.log('rxtx ret', hex);

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

    logStore.addLog('UART service connected');

    this.connected = true;
  }

  async sendRxTxCommand(command: string) {
    if (this.rxtxCharacteristic) {
      logStore.addLog(`Sending RXTX command: ${command}`);
      await this.rxtxCharacteristic.writeValueWithResponse(hexToBytes(command) as BufferSource);
    } else {
      logStore.addLog("Service unavailable. Is Bluetooth connected?");
    }
  }

  async sendEpdCommand(command: string) {
    if (this.epdCharacteristic) {
      logStore.addLog(`Sending EPD command: ${command}`);
      await this.epdCharacteristic.writeValueWithResponse(hexToBytes(command) as BufferSource);
    } else {
      logStore.addLog("Service unavailable. Is Bluetooth connected?");
    }
  }

  resetVariables() {
    this.gattServer = null;
    this.epdService = null;
    this.epdCharacteristic = null;
    this.rxtxCharacteristic = null;
    this.rxtxService = null;
    this.connected = false;
    this.preconnected = false;
    // document.getElementById("log").value = "";
  }
}

export const bleConnectionStore = new BleConnectionStore();

// Helper to match your existing error handling signature.
// If you already have one elsewhere, remove this.
async function handleError(e: unknown): Promise<void> {
  console.error(e);
  logStore.addLog('Error: ' + (e instanceof Error ? e.message : String(e)));
}
