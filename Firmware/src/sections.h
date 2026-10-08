#pragma once

// Memory placement for the TC32 linker script. RAM keeps a variable through deep-retention sleep;
// _attribute_ram_code_ runs a function from SRAM. Host builds (tools/) compile both away, which
// lets domain/ and application/ code build without the Telink SDK.
#ifdef HOST_BUILD
#define RAM
#define _attribute_ram_code_
#else
#include "drivers/8258/compiler.h"
#define RAM _attribute_data_retention_
#endif
