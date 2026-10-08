#pragma once

// OTA characteristic: stages a firmware image in the flash bank the firmware is not running from
// and boots it on request.
int ota_service_write(void *p);
// Forgets the transfer state; called when the connection changes.
void ota_service_reset(void);
