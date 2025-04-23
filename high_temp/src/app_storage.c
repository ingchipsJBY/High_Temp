#include <string.h>
#include "kv_storage.h"
#include "platform_api.h"
#include "btstack_util.h"
#include "le_device_db.h"
#include "app_mode.h"
#include "app_storage.h"
#include "app_2g4.h"

static const app_store_ble_channel_info_t channel_info_default = {
    .local_addr = {0},
    .peer_key = KV_KEY_INVALID,
};

static const app_store_ble_mode_t ble_mode_default = {
    .kmode    = MODE_BLE,  // KB_MODE_2_4G =1, KB_MODE_BLE =2
    .bchannel = BLE_CHANNEL_1,  // BLE_CHANNEL_1 =1, BLE_CHANNEL_2 =2, BLE_CHANNEL_3 =3
};

static const app_store_2g4_bond_state_t app_2g4_state_default = {
    .state = APP_2G4_BOND_STATE_NOT_BONDED,
    .addr = 0x1234567A,
};

static const app_store_ble_name_id_t ble_name_id_default = {
    .id = 0,
};

static const app_store_ble_name_t ble_name_default = {
    .name = {},
    .len = 0,
};


//=======================================================================================
void print_addr(const uint8_t *addr) {
    log_printf("addr: %02X_%02X_%02X_%02X_%02X_%02X\r\n", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
}

void print_addr2(const uint8_t *addr) {
    log_printf(" %02X_%02X_%02X_%02X_%02X_%02X ", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
}

//=======================================================================================
// *****************************************************
// 本机使用地址池，第一次上电会随机生成并存储，之后每次开启新的配对都会使用新的地址，即末尾加一，没有配对成功则地址不会增加；
// *****************************************************

#define STATIC_ADDR_FIRST_BYTE      0xCA  // 设置成静态随机地址，最高两个bit为1，这里直接将第一字节固定为CA；

/*
	初始化一个地址，六个字节 第一个字节为0XCA,并且写入flash KV_KEY_LOCAL_ADDR_POOL
	下一次上电从flash里面读出来如果有 STATIC_ADDR_FIRST_BYTE 数据就不会再次创建
*/
static void app_storage_addr_pool_init(void){
    int16_t len;
    log_printf("addr pool start1\n");
    uint8_t * address = (uint8_t *)kv_get(KV_KEY_LOCAL_ADDR_POOL, &len);
    if(address[0] != STATIC_ADDR_FIRST_BYTE || len != sizeof(bd_addr_t)){ // not exist.
        bd_addr_t new_addr;
        platform_hrng((uint8_t *)new_addr, sizeof(bd_addr_t));
        new_addr[0] = STATIC_ADDR_FIRST_BYTE; 
        kv_put(KV_KEY_LOCAL_ADDR_POOL, (const uint8_t *)new_addr, sizeof(bd_addr_t));
        log_printf("%s: generate local addr!\n", __func__);
    }
}

// 从本地地址池取出地址，用于配对设置
uint8_t app_storage_addr_pool_get(uint8_t * new_addr){
    if(new_addr == NULL){
        log_printf("%s: param error!\n", __func__);
        return 1;
    }
    uint8_t * address = (uint8_t *)kv_get(KV_KEY_LOCAL_ADDR_POOL, NULL);
    // 如果flash中不存在地址信息，则返回错误；否则，将地址拷贝走。
    if(address[0] != STATIC_ADDR_FIRST_BYTE){ // not exist.
        log_printf("%s: loc addr not exist!\n", __func__);
        return 2;
    } else { // exist.
        memcpy(new_addr, address, sizeof(bd_addr_t));
    }
    return 0;
}

// 更新地址池，将最后一个字节加一
uint8_t app_storage_addr_pool_update(void){
    log_printf("%s\n", __func__);
    uint8_t * address = (uint8_t *)kv_get(KV_KEY_LOCAL_ADDR_POOL, NULL);
    // 如果flash中不存在地址信息，则产生一次；否则，将地址拷贝走。
    if(address[0] != STATIC_ADDR_FIRST_BYTE) { // not exist.
        log_printf("%s: loc addr not exist!\n", __func__);
        return 2;
    } else { // exist.
        bd_addr_t temp_addr;
        memcpy(temp_addr, address, sizeof(bd_addr_t));
        temp_addr[5]++;
        kv_put(KV_KEY_LOCAL_ADDR_POOL, (const uint8_t *)temp_addr, sizeof(bd_addr_t));
    }
    return 0;
}


// *****************************************************
// 绑定信息存储，存储格式为 app_store_ble_channel_info_t
// *****************************************************
/*
	初始化ble band通道信息，如果flash空间为空，直接写入默认6个字节0数据的MAC地址，将蓝牙状态设置为 KV_KEY_INVALID
*/
static void app_storage_ble_bond_info_init(void){
    log_printf("%s\n", __func__);
	
    for(uint8_t channel = BLE_CHANNEL_1; channel < BLE_CHANNEL_MAX; channel++){
        int16_t len;
        app_store_ble_channel_info_t * ch_info = (app_store_ble_channel_info_t *)kv_get(KV_KEY_CHANNEL(channel), &len);
        if(ch_info == NULL || (len != sizeof(app_store_ble_channel_info_t))){
            kv_put(KV_KEY_CHANNEL(channel), (const uint8_t *)&channel_info_default, sizeof(app_store_ble_channel_info_t)); //clear
#if USE_KEY_REMAP_FOR_PAIR
            kv_remove(KV_KEY_REMAP_KEY(channel)); //清除对应通道的密钥信息，这里是database直接使用的；
#endif
        }
    }
}

// channel = 0, 1, 2
uint8_t app_storage_bond_info_write(uint8_t channel, uint8_t *loc_addr, uint8_t peerKey){
    log_printf("%s: ch[%d], peerKey:0x%02x\n", __func__, channel, peerKey);

    if(channel  == BLE_CHANNEL_INVALID || channel >= BLE_CHANNEL_MAX){
        log_printf("%s: channel[%d] error!\n", __func__, channel);
        return 1;
    }

    app_store_ble_channel_info_t * ch_info = (app_store_ble_channel_info_t *)kv_get(KV_KEY_CHANNEL(channel), NULL);
    if(ch_info == NULL) {log_printf("%s, ch[%d] info not exist.\n", __func__, channel); return 2;}


    memcpy(ch_info->local_addr, loc_addr, sizeof(bd_addr_t));
    ch_info->peer_key = KV_KEY_REMAP_KEY(channel); // peerKey 输入参数不再使用，因为总是固定为0x01了
    kv_value_modified_of_key(KV_KEY_CHANNEL(channel));

    // 更新本地地址存储池，以便于下一次绑定时使用
    app_storage_addr_pool_update();

    return 0;
}

// channel = 0, 1, 2
uint8_t app_storage_bond_state_get(uint8_t channel, uint8_t *pair_state){
    if(channel  == BLE_CHANNEL_INVALID || channel >= BLE_CHANNEL_MAX){
        log_printf("%s: channel[%d] error!\n", __func__, channel);
        return 1;
    }
    app_store_ble_channel_info_t * ch_info = (app_store_ble_channel_info_t *)kv_get(KV_KEY_CHANNEL(channel), NULL);
    if(ch_info == NULL) {log_printf("%s, ch[%d] info not exist.\n", __func__, channel); return 2;}
    if(ch_info->peer_key != KV_KEY_INVALID){
        *pair_state = 1;
    } else {
        *pair_state = 0;
    }
    return 0;
}

// 根据通道获取本地存储地址，用于本通道的广播
uint8_t app_storage_bond_info_get(uint8_t channel, uint8_t * loc_addr){
    if(channel  == BLE_CHANNEL_INVALID || channel >= BLE_CHANNEL_MAX){
        log_printf("%s: channel[%d] error!\n", __func__, channel);
        return 1;
    }
    app_store_ble_channel_info_t * ch_info = (app_store_ble_channel_info_t *)kv_get(KV_KEY_CHANNEL(channel), NULL);
    if(ch_info == NULL) {log_printf("%s, ch[%d] info not exist.\n", __func__, channel); return 2;}
    if(ch_info->peer_key != KV_KEY_INVALID){
        memcpy(loc_addr, (uint8_t *)ch_info->local_addr, sizeof(bd_addr_t));
        return 0;
    }
    return 1; // fail
}

// 根据通道删除绑定信息
uint8_t app_storage_bond_info_clear(uint8_t channel){
    log_printf("%s: ch[%d] \n", __func__, channel);
    if(channel  == BLE_CHANNEL_INVALID || channel >= BLE_CHANNEL_MAX){
        log_printf("%s: channel[%d] error!\n", __func__, channel);
        return 1;
    }
    app_store_ble_channel_info_t * ch_info = (app_store_ble_channel_info_t *)kv_get(KV_KEY_CHANNEL(channel), NULL);
    if(ch_info == NULL) {log_printf("%s, ch[%d] info not exist.\n", __func__, channel); return 2;}
    if(ch_info->peer_key != KV_KEY_INVALID){
#if USE_KEY_REMAP_FOR_PAIR
        log_printf("%s: channel[%d] clear bonding info!\n", __func__, channel);
        kv_remove(KV_KEY_REMAP_KEY(channel)); //清除对应通道的密钥信息；
#else
        log_printf("%s: channel[%d] clear le dev db info!\n", __func__, channel);
        le_device_db_remove_key(ch_info->peer_key);
#endif
    }
    // clear channel info.
    kv_put(KV_KEY_CHANNEL(channel), (const uint8_t *)&channel_info_default, sizeof(app_store_ble_channel_info_t)); //clear
    return 0;
}

// 清除所有通道的绑定信息
void app_storage_bond_info_clear_all(void){
    for(uint8_t channel=BLE_CHANNEL_1; channel<BLE_CHANNEL_MAX; channel++){
        app_storage_bond_info_clear(channel);
    }
}

// 打印所有通道的绑定信息
void print_all_channel_bond_info(void){
    for(uint8_t channel=BLE_CHANNEL_1; channel<BLE_CHANNEL_MAX; channel++){
        app_store_ble_channel_info_t * ch_info = (app_store_ble_channel_info_t *)kv_get(KV_KEY_CHANNEL(channel), NULL);
        if(ch_info == NULL) {log_printf("ch[%d] not exist.\n", channel); return;}
        if(ch_info->peer_key != KV_KEY_INVALID){
            log_printf("channel[%d] local_addr: ", channel);
            print_addr2(ch_info->local_addr);
            log_printf("peer_key: %d\n", ch_info->peer_key);
        } else {
            log_printf("channel[%d] not exist.\n", channel);
        }
    }
}

// *****************************************************
// 工作模式存储
// *****************************************************
static uint8_t is_kb_mode_valid(uint8_t kb_mod){
    if (kb_mod != MODE_2G4 && kb_mod != MODE_BLE && kb_mod != MODE_USB)
        return 0;
    return 1;
}

/*
	上电获取flash里面的工作的模式以及蓝牙对应的通道
	如果是空数据则直接初始化为蓝牙模式，通道1
*/
static void app_storage_ble_mode_init(void){
	log_printf("%s\n", __func__);
	
    int16_t len;
    app_store_ble_mode_t * kb_mode = (app_store_ble_mode_t *)kv_get(KV_KEY_KB_MODE, &len);
    if(kb_mode == NULL || (len != sizeof(app_store_ble_mode_t))){
        kv_put(KV_KEY_KB_MODE, (const uint8_t *)&ble_mode_default, sizeof(app_store_ble_mode_t)); //clear
    }    
    else if (!is_kb_mode_valid(kb_mode->kmode)){
        kb_mode->kmode = ble_mode_default.kmode;
        kv_value_modified_of_key(KV_KEY_KB_MODE);
    }
    else if (kb_mode->bchannel  == 0 || kb_mode->bchannel > BLE_CHANNEL_MAX){
        kb_mode->bchannel = ble_mode_default.bchannel;
        kv_value_modified_of_key(KV_KEY_KB_MODE);
    }
}

static void app_storage_remove_all(void){
    log_printf("remove all kv.\n");
    kv_remove_all();
}

uint8_t app_storage_mode_read(void){
    int16_t len;
    app_store_ble_mode_t * kb_mode = (app_store_ble_mode_t *)kv_get(KV_KEY_KB_MODE, &len);
    if(kb_mode != NULL && len == sizeof(app_store_ble_mode_t)){
        return kb_mode->kmode; // KB_MODE_2_4G=1, KB_MODE_BLE=2
    }
    return 0; // WIRELESS_MODE_INVALID
}

uint8_t app_storage_ble_channel_read(void){
    int16_t len;
    app_store_ble_mode_t * kb_mode = (app_store_ble_mode_t *)kv_get(KV_KEY_KB_MODE, &len);
    if(kb_mode != NULL && len == sizeof(app_store_ble_mode_t)){
        return kb_mode->bchannel; // BLE_CHANNEL_1=1, BLE_CHANNEL_2=2, BLE_CHANNEL_3=3
    }
    return 0; // BLE_CHANNEL_INVALID
}

uint8_t app_storage_mode_write(uint8_t kb_mod){
    if (!is_kb_mode_valid(kb_mod)){
        log_printf("unsupport kb mode:%d\n", kb_mod);
        return 2;
    }
        
    int16_t len;
    app_store_ble_mode_t * kb_mode = (app_store_ble_mode_t *)kv_get(KV_KEY_KB_MODE, &len);
    if(kb_mode != NULL && len == sizeof(app_store_ble_mode_t)){
        kb_mode->kmode = kb_mod;
        kv_value_modified_of_key(KV_KEY_KB_MODE);
        return 0;
    }
    return 1;
}

// channel = 1,2,3
uint8_t app_storage_ble_channel_write(uint8_t channel){
    if (channel  == BLE_CHANNEL_INVALID || channel >= BLE_CHANNEL_MAX){
        log_printf("unsupport channel:%d\n", channel);
        return 2;
    }
        
    int16_t len;
    app_store_ble_mode_t * kb_mode = (app_store_ble_mode_t *)kv_get(KV_KEY_KB_MODE, &len);
    if(kb_mode != NULL && len == sizeof(app_store_ble_mode_t)){
        kb_mode->bchannel = channel;
        kv_value_modified_of_key(KV_KEY_KB_MODE);
        return 0;
    }
    return 1;
}

/*先读取绑定信息，如果里面空数据，或者没有配对信息，就往里面写初始化绑定信息*/
void app_storage_2g4_state_init(void){
    int16_t len;
	
	log_printf("[2.4G] %s \n", __func__);
    app_store_2g4_bond_state_t * app_2g4_state = (app_store_2g4_bond_state_t *)kv_get(KV_KEY_2G4_BOND_STATE, &len);
    if(app_2g4_state == NULL || (len != sizeof(app_store_2g4_bond_state_t))){
        kv_put(KV_KEY_2G4_BOND_STATE, (const uint8_t *)&app_2g4_state_default, sizeof(app_store_2g4_bond_state_t)); //clear
    }
}

/*获取绑定状态信息，返回值为绑定状态，如果数据为空 返回0*/
uint8_t app_storage_2g4_state_read(void){
    int16_t len;
    app_store_2g4_bond_state_t * app_2g4_state = (app_store_2g4_bond_state_t *)kv_get(KV_KEY_2G4_BOND_STATE, &len);
    if(app_2g4_state != NULL && len == sizeof(app_store_2g4_bond_state_t)){
        return app_2g4_state->state; // KB_MODE_2_4G=1, KB_MODE_BLE=2
    }
    return 0; // invalid state
}

/*获取绑定地址信息，返回值为绑定地址，如果数据为空 返回0*/
uint32_t app_storage_2g4_addr_read(void){
    int16_t len;
    app_store_2g4_bond_state_t * app_2g4_state = (app_store_2g4_bond_state_t *)kv_get(KV_KEY_2G4_BOND_STATE, &len);
    if(app_2g4_state != NULL && len == sizeof(app_store_2g4_bond_state_t)){
        return app_2g4_state->addr; // KB_MODE_2_4G=1, KB_MODE_BLE=2
    }
    return 0; // WIRELESS_MODE_INVALID
}

/*	
	将需要写入的绑定地址数据写入flash
	1、获取flash里面是否有数据，如果数据为空或者获取的长度不对则返回 1
	2、发送写入报告，然后再其他回调写入flash 返回0
*/
uint8_t app_storage_2g4_addr_write(uint32_t addr){
    int16_t len;
    app_store_2g4_bond_state_t * app_2g4_state = (app_store_2g4_bond_state_t *)kv_get(KV_KEY_2G4_BOND_STATE, &len);
    if(app_2g4_state != NULL && len == sizeof(app_store_2g4_bond_state_t)){
        app_2g4_state->state = APP_2G4_BOND_STATE_BONDED; //bonded
        app_2g4_state->addr = addr;
        kv_value_modified_of_key(KV_KEY_2G4_BOND_STATE);
        return 0;
    }
    return 1;
}

/*
	复位绑定信息，如果数据为空或者数据本来就是初始数据，或者获取地址出问题就返回，不复位
	先擦除flash数据，然后写入初始化数据
*/
void app_storage_2g4_state_reset(void)
{
    int16_t len;
    log_printf("%s\n", __func__);

    app_store_2g4_bond_state_t * app_2g4_state = (app_store_2g4_bond_state_t *)kv_get(KV_KEY_2G4_BOND_STATE, &len);
    if((app_2g4_state == NULL) || (app_2g4_state->state == APP_2G4_BOND_STATE_INVALID) || (app_2g4_state->state > APP_2G4_BOND_STATE_MAX))
    {
        log_printf("%s, info not exist.\n", __func__); 
        return;
    }

    kv_remove(KV_KEY_2G4_BOND_STATE); //清除对应通道的密钥信息；

    log_printf("2.4G bond info clear\n");

    // clear channel info.
    kv_put(KV_KEY_2G4_BOND_STATE, (const uint8_t *)&app_2g4_state_default, sizeof(app_store_2g4_bond_state_t));
    return;
}

static void app_storage_ble_name_id_init(void){
	log_printf("%s\n", __func__);
	
    int16_t len;
    app_store_ble_name_id_t * ble_name_id = (app_store_ble_name_id_t *)kv_get(KV_KEY_BLE_NAME_ID, &len);
    if(ble_name_id == NULL || (len != sizeof(app_store_ble_name_id_t))){
        kv_put(KV_KEY_BLE_NAME_ID, (const uint8_t *)&ble_name_id_default, sizeof(ble_name_id_default)); //clear
    }
}

uint16_t app_storage_ble_name_id_read(void){
    int16_t len;
    app_store_ble_name_id_t * ble_name_id = (app_store_ble_name_id_t *)kv_get(KV_KEY_BLE_NAME_ID, &len);
    if(ble_name_id != NULL && len == sizeof(app_store_ble_name_id_t)){
        return ble_name_id->id;
    }
    return BLE_NAME_ID_INVALID;
}

uint8_t app_storage_ble_name_id_write(uint16_t id){ 
    int16_t len;
    app_store_ble_name_id_t * ble_name_id = (app_store_ble_name_id_t *)kv_get(KV_KEY_BLE_NAME_ID, &len);
    if(ble_name_id != NULL && len == sizeof(app_store_ble_name_id_t)){
        ble_name_id->id = id;
        kv_value_modified_of_key(KV_KEY_BLE_NAME_ID);
        return 0;
    }
    return 1;
}

static void app_storage_ble_name_init(void){
	log_printf("%s\n", __func__);
	
    int16_t len;
    app_store_ble_name_t * ble_name = (app_store_ble_name_t *)kv_get(KV_KEY_BLE_NAME_CH1, &len);
    if(ble_name == NULL || (ble_name->len >= MAX_NAME_LEN)){
        kv_put(KV_KEY_BLE_NAME_CH1, (const uint8_t *)&ble_name_default, sizeof(app_store_ble_name_t)); //clear
    }

    ble_name = (app_store_ble_name_t *)kv_get(KV_KEY_BLE_NAME_CH2, &len);
    if(ble_name == NULL || (ble_name->len >= MAX_NAME_LEN)){
        kv_put(KV_KEY_BLE_NAME_CH2, (const uint8_t *)&ble_name_default, sizeof(app_store_ble_name_t)); //clear
    }

    ble_name = (app_store_ble_name_t *)kv_get(KV_KEY_BLE_NAME_CH3, &len);
    if(ble_name == NULL || (ble_name->len >= MAX_NAME_LEN)){
        kv_put(KV_KEY_BLE_NAME_CH3, (const uint8_t *)&ble_name_default, sizeof(app_store_ble_name_t)); //clear
    }
    
}

uint8_t app_storage_ble_ch1_name_read(char *name){
    int16_t len;
    app_store_ble_name_t * ble_name = (app_store_ble_name_t *)kv_get(KV_KEY_BLE_NAME_CH1, &len);
    if(ble_name != NULL && len == sizeof(app_store_ble_name_t)){
        memcpy(name, (char *)ble_name->name, sizeof(ble_name_t));
        return 0;
    }
    return 1; // fail
}

uint8_t app_storage_ble_ch1_name_len_read(void){
    int16_t len;
    app_store_ble_name_t * ble_name = (app_store_ble_name_t *)kv_get(KV_KEY_BLE_NAME_CH1, &len);
    if(ble_name != NULL && len == sizeof(app_store_ble_name_t)){
        return ble_name->len; 
    }
    return BLE_NAME_LEN_INVALID; // BLE_NAME_CH1_INVALID
}

uint8_t app_storage_ble_ch1_name_len_write(uint8_t length){ 
    int16_t len;
    app_store_ble_name_t * ble_name = (app_store_ble_name_t *)kv_get(KV_KEY_BLE_NAME_CH1, &len);
    if(ble_name != NULL && len == sizeof(app_store_ble_name_t)){
        ble_name->len = length;
        kv_value_modified_of_key(KV_KEY_BLE_NAME_CH1);
        return 0;
    }
    return 1;
}

uint8_t app_storage_ble_ch1_name_write(char *name){
    int16_t len;
    app_store_ble_name_t * ble_name = (app_store_ble_name_t *)kv_get(KV_KEY_BLE_NAME_CH1, &len);
    if(ble_name != NULL && len == sizeof(app_store_ble_name_t)){
        memcpy(ble_name->name, (uint8_t *)name, sizeof(ble_name_t));
        kv_value_modified_of_key(KV_KEY_BLE_NAME_CH1);
        return 0;
    }
    return 1;
}

uint8_t app_storage_ble_ch2_name_read(char *name){
    int16_t len;
    app_store_ble_name_t * ble_name = (app_store_ble_name_t *)kv_get(KV_KEY_BLE_NAME_CH2, &len);
    if(ble_name != NULL && len == sizeof(app_store_ble_name_t)){
        memcpy(name, (char *)ble_name->name, sizeof(ble_name_t));
        return 0;
    }
    return 1; // fail
}

uint8_t app_storage_ble_ch2_name_len_read(void){
    int16_t len;
    app_store_ble_name_t * ble_name = (app_store_ble_name_t *)kv_get(KV_KEY_BLE_NAME_CH2, &len);
    if(ble_name != NULL && len == sizeof(app_store_ble_name_t)){
        return ble_name->len; 
    }
    return BLE_NAME_LEN_INVALID; // BLE_NAME_CH1_INVALID
}

uint8_t app_storage_ble_ch2_name_len_write(uint8_t length){ 
    int16_t len;
    app_store_ble_name_t * ble_name = (app_store_ble_name_t *)kv_get(KV_KEY_BLE_NAME_CH2, &len);
    if(ble_name != NULL && len == sizeof(app_store_ble_name_t)){
        ble_name->len = length;
        kv_value_modified_of_key(KV_KEY_BLE_NAME_CH2);
        return 0;
    }
    return 1;
}

uint8_t app_storage_ble_ch2_name_write(char *name){ 
    int16_t len;
    app_store_ble_name_t * ble_name = (app_store_ble_name_t *)kv_get(KV_KEY_BLE_NAME_CH2, &len);
    if(ble_name != NULL && len == sizeof(app_store_ble_name_t)){
        memcpy(ble_name->name, (uint8_t *)name, sizeof(ble_name_t));
        kv_value_modified_of_key(KV_KEY_BLE_NAME_CH2);
        return 0;
    }
    return 1;
}

uint8_t app_storage_ble_ch3_name_read(char *name){
    int16_t len;
    app_store_ble_name_t * ble_name = (app_store_ble_name_t *)kv_get(KV_KEY_BLE_NAME_CH3, &len);
    if(ble_name != NULL && len == sizeof(app_store_ble_name_t)){
        memcpy(name, (char *)ble_name->name, sizeof(ble_name_t));
        return 0;
    }
    return 1; // fail
}

uint8_t app_storage_ble_ch3_name_len_read(void){
    int16_t len;
    app_store_ble_name_t * ble_name = (app_store_ble_name_t *)kv_get(KV_KEY_BLE_NAME_CH3, &len);
    if(ble_name != NULL && len == sizeof(app_store_ble_name_t)){
        return ble_name->len; 
    }
    return BLE_NAME_LEN_INVALID; // BLE_NAME_CH1_INVALID
}

uint8_t app_storage_ble_ch3_name_len_write(uint8_t length){ 
    int16_t len;
    app_store_ble_name_t * ble_name = (app_store_ble_name_t *)kv_get(KV_KEY_BLE_NAME_CH3, &len);
    if(ble_name != NULL && len == sizeof(app_store_ble_name_t)){
        ble_name->len = length;
        kv_value_modified_of_key(KV_KEY_BLE_NAME_CH3);
        return 0;
    }
    return 1;
}

uint8_t app_storage_ble_ch3_name_write(char *name){ 
    int16_t len;
    app_store_ble_name_t * ble_name = (app_store_ble_name_t *)kv_get(KV_KEY_BLE_NAME_CH3, &len);
    if(ble_name != NULL && len == sizeof(app_store_ble_name_t)){
        memcpy(ble_name->name, (uint8_t *)name, sizeof(ble_name_t));
        kv_value_modified_of_key(KV_KEY_BLE_NAME_CH3);
        return 0;
    }
    return 1;
}
//=======================================================================================
// *****************************************************
// kv storage test function
// *****************************************************
static int kv_visit_dump(const kvkey_t key, const uint8_t *data, const int16_t len, void *user_data){
    log_printf("k = %02X:\nv = ", key);
    printf_hexdump(data, len);
    log_printf("\n");
    return KV_OK;
}

void all_kv_dump(void){
    kv_visit(kv_visit_dump, NULL);
}

void print_all_device_db(void) {
    log_printf("[WARNING] YOU CAN GET ONLY ONE DB!\n");

    le_device_memory_db_iter_t iter;
    le_device_db_iter_init(&iter);
    while (le_device_db_iter_next(&iter)) {
        le_device_memory_db_t *cur = le_device_db_iter_cur(&iter);
        log_printf("[db]: key:%d peer_addr_type:%d, peer_addr: ", iter.key, cur->addr_type);
        print_addr2(cur->addr);
        log_printf(",irk: ");
        for(uint8_t i=0; i<sizeof(sm_key_t); i++){
            log_printf("%02X ", cur->irk[i]);
        }
        log_printf("\n");
    }
}


//=======================================================================================
void app_storage_init(void) {
    
	/*初始化 KV_IMPL 架构地址*/
	kv_impl_init();
	
	/*获取 0xCA 为开头的六个字节的地址*/
    app_storage_addr_pool_init();
	
	/*初始化蓝牙三个通道的Band信息以及MAC地址*/
    app_storage_ble_bond_info_init();
	
	/*获取flash里面存储的工作模式以及蓝牙通道*/	
    app_storage_ble_mode_init();
	
	/*获取2.4G里面的绑定信息以及2.4G的绑定地址*/
    app_storage_2g4_state_init();

	/*初始化蓝牙名称，默认是空字符*/
    app_storage_ble_name_id_init();
	
    app_storage_ble_name_init();
}


/**
 * @brief 恢复出厂设置
 * @note 
 * 1. 蓝牙配对信息会被清除；
 * 2. dongle配对信息不会被清除；
 * 3. 常规灯效记忆状态会恢复；
 * 
 */
void app_storage_reset_to_default(void){
    app_storage_remove_all();
    app_storage_addr_pool_init();
    app_storage_ble_bond_info_init();
    app_storage_ble_mode_init();
    all_kv_dump();
    print_all_device_db();
}

