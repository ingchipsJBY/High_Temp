#ifndef __APP_STORAGE_H_
#define __APP_STORAGE_H_

#include <stdint.h>
#include "kv_storage.h"
#include "btstack_util.h"
#include "kv_impl.h"
#include "profile.h"

//key map
#define APP_STORAGE_KEY_START           (KV_USER_KEY_START)
#define APP_STORAGE_KEY_END             (KV_USER_KEY_END-1)
#define KV_KEY_INVALID                  (KV_USER_KEY_END)

#define BLE_NAME_ID_INVALID             0xFFFF
#define BLE_NAME_LEN_INVALID            0xFF
enum
{
    KV_KEY_CHANNEL_1 = APP_STORAGE_KEY_START, //0xC9, 201 // 存 app_store_ble_channel_info_t
    KV_KEY_CHANNEL_2,
    KV_KEY_CHANNEL_3,

    KV_KEY_REMAP_CHANNEL_1 = (APP_STORAGE_KEY_START+10), //0xD3, 211  // 重映射存储channel_1的配对信息，需打开 USE_KEY_REMAP_FOR_PAIR 开关才能生效.
    KV_KEY_REMAP_CHANNEL_2,
    KV_KEY_REMAP_CHANNEL_3,

    KV_KEY_LOCAL_ADDR_POOL = (APP_STORAGE_KEY_START+20), //0xDD, 221 //本地地址池
    KV_KEY_KB_MODE,  // 222 存储键盘模式和通道
    KV_KEY_LED_MODE, // 223 存储led灯常规灯效模式
    KV_KEY_2G4_BOND_STATE, //224 存储2.4G绑定信息
    KV_KEY_BLE_NAME_ID, //225
    KV_KEY_BLE_NAME_CH1, //226
    KV_KEY_BLE_NAME_CH2, //227
    KV_KEY_BLE_NAME_CH3, //228
};

#define KV_KEY_CHANNEL(x)               (KV_KEY_CHANNEL_1 + x - 1) // x=1,2,3

#define KV_KEY_REMAP_KEY(x)             (KV_KEY_REMAP_CHANNEL_1 + x - 1) // x=1,2,3
#define KV_KEY_REMAP_KEY_UPDATE(x)      key_remap_key_set(KV_KEY_REMAP_KEY(x)) // x=1,2,3  更新底层配对映射key值 

// struct.
typedef struct __attribute__((packed)) {
    bd_addr_t           local_addr;
    uint8_t             peer_key;
} app_store_ble_channel_info_t;

typedef struct __attribute__((packed)) {
    uint32_t           local_addr;
} app_store_2g4_info_t;

typedef struct __attribute__((packed)) {
    uint8_t kmode;
    uint8_t bchannel;
} app_store_ble_mode_t;

typedef struct __attribute__((packed)) {
    uint8_t state;  //1- not bond; 2-bonded; 0-error
    uint32_t addr;
} app_store_2g4_bond_state_t;

typedef struct __attribute__((packed)) {
    uint16_t id;
} app_store_ble_name_id_t;

typedef struct __attribute__((packed)) {
    ble_name_t name;
    uint8_t len;
} app_store_ble_name_t;

void print_addr(const uint8_t *addr);
void print_addr2(const uint8_t *addr);

uint8_t app_storage_mode_read(void);
uint8_t app_storage_ble_channel_read(void);
uint8_t app_storage_mode_write(uint8_t kb_mod);
uint8_t app_storage_ble_channel_write(uint8_t channel);

uint8_t app_storage_addr_pool_get(uint8_t * new_addr);

uint8_t app_storage_bond_info_write(uint8_t channel, uint8_t *loc_addr, uint8_t peerKey);
uint8_t app_storage_bond_state_get(uint8_t channel, uint8_t *pair_state);
uint8_t app_storage_bond_info_get(uint8_t channel, uint8_t * loc_addr);

uint16_t app_storage_ble_name_id_read(void);
uint8_t app_storage_ble_name_id_write(uint16_t id);

uint8_t app_storage_ble_ch1_name_read(char *name);
uint8_t app_storage_ble_ch1_name_len_read(void);
uint8_t app_storage_ble_ch1_name_len_write(uint8_t length);
uint8_t app_storage_ble_ch1_name_write(char *name);

uint8_t app_storage_ble_ch2_name_read(char *name);
uint8_t app_storage_ble_ch2_name_len_read(void);
uint8_t app_storage_ble_ch2_name_len_write(uint8_t length);
uint8_t app_storage_ble_ch2_name_write(char *name);

uint8_t app_storage_ble_ch3_name_read(char *name);
uint8_t app_storage_ble_ch3_name_len_read(void);
uint8_t app_storage_ble_ch3_name_len_write(uint8_t length);
uint8_t app_storage_ble_ch3_name_write(char *name);
uint8_t app_storage_2g4_state_read(void);
uint32_t app_storage_2g4_addr_read(void);
uint8_t app_storage_2g4_addr_write(uint32_t addr);
void app_storage_2g4_state_reset(void);
void app_storage_2g4_state_init(void);
void app_storage_init(void);
void app_storage_reset_to_default(void);

#endif
