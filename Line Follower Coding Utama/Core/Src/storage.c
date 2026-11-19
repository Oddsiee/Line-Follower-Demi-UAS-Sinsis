#include "storage.h"
#include "config.h"
#include "stm32f4xx_hal.h"
#include <stddef.h>
#include <string.h>

static uint32_t compute_checksum(const Params_t *p)
{
    const uint8_t *bytes = (const uint8_t *)p;
    size_t len = offsetof(Params_t, checksum);
    uint32_t hash = 2166136261UL; /* FNV-1a 32-bit */
    for (size_t i = 0U; i < len; i++) {
        hash ^= (uint32_t)bytes[i];
        hash *= 16777619UL;
    }
    return hash;
}

bool Storage_Load(Params_t *p)
{
    if (p == NULL) {
        return false;
    }

    Params_t tmp;
    memcpy(&tmp, (const void *)FLASH_PARAMS_ADDR, sizeof(Params_t));

    if (tmp.magic != PARAMS_MAGIC) {
        return false;
    }
    if (tmp.checksum != compute_checksum(&tmp)) {
        return false;
    }

    memcpy(p, &tmp, sizeof(Params_t));
    return true;
}

bool Storage_Save(Params_t *p)
{
    if (p == NULL) {
        return false;
    }

    p->magic    = PARAMS_MAGIC;
    p->checksum = compute_checksum(p);

    if (HAL_FLASH_Unlock() != HAL_OK) {
        return false;
    }

    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP    | FLASH_FLAG_OPERR  |
                           FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR |
                           FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

    FLASH_EraseInitTypeDef erase = {0};
    uint32_t sector_err = 0U;
    erase.TypeErase    = FLASH_TYPEERASE_SECTORS;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    erase.Sector       = FLASH_PARAMS_SECTOR;
    erase.NbSectors    = 1U;

    if (HAL_FLASHEx_Erase(&erase, &sector_err) != HAL_OK) {
        HAL_FLASH_Lock();
        return false;
    }

    const uint8_t *bytes = (const uint8_t *)p;
    size_t total = sizeof(Params_t);
    uint32_t addr = FLASH_PARAMS_ADDR;
    bool ok = true;

    for (size_t off = 0U; off < total; off += 4U) {
        uint32_t word = 0xFFFFFFFFUL;
        size_t chunk = ((total - off) >= 4U) ? 4U : (total - off);
        memcpy(&word, bytes + off, chunk);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + (uint32_t)off, word) != HAL_OK) {
            ok = false;
            break;
        }
    }

    HAL_FLASH_Lock();

    if (!ok) {
        return false;
    }

    return (memcmp((const void *)FLASH_PARAMS_ADDR, p, sizeof(Params_t)) == 0);
}
