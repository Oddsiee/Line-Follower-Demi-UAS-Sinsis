#include "storage.h"

/* TODO (M7, pemilik C): HAL_FLASH_Unlock, erase sektor 5 (FLASH_PARAMS_SECTOR / FLASH_PARAMS_ADDR = 0x08020000UL pada STM32F411CCU6 256 KB),
 * tulis Params_t per word, validasi magic + checksum saat load. */

bool Storage_Load(Params_t *p) { (void)p; return false; }
bool Storage_Save(Params_t *p) { (void)p; return false; }
