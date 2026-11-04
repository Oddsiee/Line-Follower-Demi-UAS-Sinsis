#include "storage.h"

/* TODO (M7, pemilik C): HAL_FLASH_Unlock, erase sektor 7 (FLASH_PARAMS_ADDR),
 * tulis Params_t per word, validasi magic + checksum saat load. */

bool Storage_Load(Params_t *p) { (void)p; return false; }
bool Storage_Save(Params_t *p) { (void)p; return false; }
