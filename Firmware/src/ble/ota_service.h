#pragma once

// OTA characteristic: stages a firmware image in the upper flash bank and applies it on request.
int ota_service_write(void *p);
// Forgets the transfer state; called when the connection changes.
void ota_service_reset(void);
