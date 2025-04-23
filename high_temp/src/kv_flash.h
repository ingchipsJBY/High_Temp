#ifndef __KV_FLASH_H
#define __KV_FLASH_H

#include "stdint.h"

typedef enum : uint8_t
{
	CLEAR_CONNECT_INFO  = 0x5B
}command_unite;

typedef enum
{
    RESET_CHECK = 0,
    CONNECT_CHECK,
}kv_key_t;

typedef struct ADV_LOOP
{
    uint8_t loop_cnt;
    uint8_t conn_cnt;
    uint8_t ascii_cnt[4];
}adv_loop_t;



void check_flash(adv_loop_t * data);
uint8_t read_rest_flash(adv_loop_t * data ,kv_key_t key);
void get_con_flash(adv_loop_t * data);


#endif
