import { bleConnectionStore } from './connectionStore.svelte';
import { serialFlashStore } from './serialStore.svelte';

// Whether a serial flash, Bluetooth firmware update or image upload is running. Leaving the page or
// switching section while one runs would cut it off.
export const transfers = {
	get active() {
		return bleConnectionStore.busy || serialFlashStore.busy;
	}
};
