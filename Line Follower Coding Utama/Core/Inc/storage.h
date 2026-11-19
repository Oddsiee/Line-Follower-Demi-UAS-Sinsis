#ifndef STORAGE_H
#define STORAGE_H

#include <stdbool.h>
#include <stdint.h>
#include "params.h"

/* ============================================================
 * storage.h — Penyimpanan Params_t & kalibrasi ke Flash Sektor 5
 * Alamat: 0x08020000UL (128 KB, aman di STM32F401CCU6 / F411CCU6 256 KB).
 * Panggil Storage_Save() hanya saat motor STOP/IDLE.
 * ============================================================ */

bool Storage_Load(Params_t *p);
bool Storage_Save(Params_t *p);

#endif /* STORAGE_H */
