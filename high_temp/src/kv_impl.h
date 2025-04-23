
/*
** COPYRIGHT (c) 2023 by INGCHIPS
*/

#ifndef __KV_IMPL_H__
#define __KV_IMPL_H__

#include "stdint.h"

#if defined __cplusplus
    extern "C" {
#endif

#define PRIVATE_FLASH_DATA_START_ADD 0x2041000
#define DB_FLASH_ADDRESS             0x2042000
#define PRIVATE_FlASH_DATA_IS_INIT   0xA55A0201

void kv_impl_init(void);

#if defined __cplusplus
    }
#endif

#endif
