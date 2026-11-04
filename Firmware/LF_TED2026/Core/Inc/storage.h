#ifndef STORAGE_H
#define STORAGE_H

#include <stdint.h>
#include <stdbool.h>
#include "params.h"

/* Pemilik: C. Flash sektor 7. Erase menghentikan CPU sesaat:
 * panggil hanya saat state IDLE. */
bool Storage_Load(Params_t *p);        /* false jika magic/checksum salah */
bool Storage_Save(Params_t *p);

#endif /* STORAGE_H */
