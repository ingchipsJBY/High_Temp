#include <stdint.h>
#include <stdio.h>
#include "platform_api.h"
#include "att_db.h"
#include "gap.h"
#include "btstack_event.h"
#include "btstack_defines.h"
#include "ad_parser.h"
#include "profile.h"
#include "kv_flash.h"
#include "kv_impl.h"
#include "kv_storage.h"

// GATT characteristic handles
#include "../data/gatt.const"

// const static uint8_t adv_data[] = {
//     #include "../data/advertising.adv"
// };

#include "../data/advertising.const"

const static uint8_t scan_data[] = {
    #include "../data/scan_response.adv"
};

#include "../data/scan_response.const"

const static uint8_t profile_data[] = {
    #include "../data/gatt.profile"
};

adv_loop_t loop_count = {0, 0, {0}};

uint8_t adv_data[] = {
//device name 
    17, 0x09,
	0x74, 0x65, 0x6D, 0x70, 0x39, 0x5F, 0x6C, 0x6F, 0x6F, 
	0x70, 0X5F, 0x00, 0x00, 0x00, 0x5F, 0x70
};


uint8_t find_change_adv(void)
{
    uint8_t offset = 0;
    uint16_t length = 0;
    uint8_t * temp = (uint8_t *)ad_data_from_type(sizeof(adv_data) , (uint8_t *)adv_data, 0x09 ,&length);
    platform_printf("length is %d\n",length);
    if(temp == NULL || length < 14){
        return 1;
    }
    else{
        offset += (length - 1);
                adv_data[offset + 2] = loop_count.ascii_cnt[3];
				adv_data[offset--] = loop_count.ascii_cnt[2];
				adv_data[offset--] = loop_count.ascii_cnt[1];
				adv_data[offset] = loop_count.ascii_cnt[0];
				platform_printf("loop is %d\n",adv_data[length + 1]);
    }
		return 0;
}

void reset_timer_callback(void)
{
	platform_reset();
}

static uint16_t att_read_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t offset,
                                  uint8_t * buffer, uint16_t buffer_size)
{
    switch (att_handle)
    {

    default:
        return 0;
    }
}

static int att_write_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t transaction_mode,
                              uint16_t offset, const uint8_t *buffer, uint16_t buffer_size)
{
    platform_printf("receive %x\n",*buffer);
    switch (att_handle)
    {
        case HANDLE_CLEAR_KV_FLASH:
        {
            if(*buffer == CLEAR_CONNECT_INFO)
            {
                kv_remove(CONNECT_CHECK);
								platform_set_timer(reset_timer_callback, 5*1600);
            }
            
        }
    default:
        return 0;
    }
}

static void user_msg_handler(uint32_t msg_id, void *data, uint16_t size)
{
    switch (msg_id)
    {
        // add your code
    //case MY_MESSAGE_ID:
    //    break;
    default:
        ;
    }
}

const static ext_adv_set_en_t adv_sets_en[] = { {.handle = 0, .duration = 0, .max_events = 0} };

static void setup_adv(void)
{
    gap_set_ext_adv_para(0,
                            CONNECTABLE_ADV_BIT | SCANNABLE_ADV_BIT | LEGACY_PDU_BIT,
                            0x00a1, 0x00a1,            // Primary_Advertising_Interval_Min, Primary_Advertising_Interval_Max
                            PRIMARY_ADV_ALL_CHANNELS,  // Primary_Advertising_Channel_Map
                            BD_ADDR_TYPE_LE_RANDOM,    // Own_Address_Type
                            BD_ADDR_TYPE_LE_PUBLIC,    // Peer_Address_Type (ignore)
                            NULL,                      // Peer_Address      (ignore)
                            ADV_FILTER_ALLOW_ALL,      // Advertising_Filter_Policy
                            0x00,                      // Advertising_Tx_Power
                            PHY_1M,                    // Primary_Advertising_PHY
                            0,                         // Secondary_Advertising_Max_Skip
                            PHY_1M,                    // Secondary_Advertising_PHY
                            0x00,                      // Advertising_SID
                            0x00);                     // Scan_Request_Notification_Enable
    gap_set_ext_adv_data(0, sizeof(adv_data), (uint8_t*)adv_data);
    gap_set_ext_scan_response_data(0, sizeof(scan_data), (uint8_t*)scan_data);
    gap_set_ext_adv_enable(1, sizeof(adv_sets_en) / sizeof(adv_sets_en[0]), adv_sets_en);
}

static void user_packet_handler(uint8_t packet_type, uint16_t channel, const uint8_t *packet, uint16_t size)
{
    static const bd_addr_t rand_addr = { 0xC9, 0xA5, 0x08, 0xFC, 0x07, 0xD8 };
    uint8_t event = hci_event_packet_get_type(packet);
    const btstack_user_msg_t *p_user_msg;
    if (packet_type != HCI_EVENT_PACKET) return;

    switch (event)
    {
    case BTSTACK_EVENT_STATE:
        if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING)
            break;
        gap_set_adv_set_random_addr(0, rand_addr);
        setup_adv();
        break;

    case HCI_EVENT_COMMAND_COMPLETE:
        switch (hci_event_command_complete_get_command_opcode(packet))
        {
        // add your code to check command complete response
        // case :
        //     break;
        default:
            break;
        }
        break;

    case HCI_EVENT_LE_META:
        switch (hci_event_le_meta_get_subevent_code(packet))
        {
        case HCI_SUBEVENT_LE_ENHANCED_CONNECTION_COMPLETE:
        case HCI_SUBEVENT_LE_ENHANCED_CONNECTION_COMPLETE_V2:
            att_set_db(decode_hci_le_meta_event(packet, le_meta_event_enh_create_conn_complete_t)->handle,
                       profile_data);
                        loop_count.conn_cnt++;
                        platform_printf("cnt is %d\n",loop_count.conn_cnt);
						kv_remove(RESET_CHECK);
                        kv_put(CONNECT_CHECK, (const uint8_t *)&loop_count.conn_cnt, sizeof(uint8_t));
                        kv_value_modified_of_key(CONNECT_CHECK);
            break;
        default:
            break;
        }

        break;

    case HCI_EVENT_DISCONNECTION_COMPLETE:
        gap_set_ext_adv_enable(1, sizeof(adv_sets_en) / sizeof(adv_sets_en[0]), adv_sets_en);
        break;

    case ATT_EVENT_CAN_SEND_NOW:
        // add your code
        break;

    case BTSTACK_EVENT_USER_MSG:
        p_user_msg = hci_event_packet_get_user_msg(packet);
        user_msg_handler(p_user_msg->msg_id, p_user_msg->data, p_user_msg->len);
        break;

    default:
        break;
    }
}

static btstack_packet_callback_registration_t hci_event_callback_registration;

uint32_t setup_profile(void *data, void *user_data)
{
    platform_printf("setup profile\n");
    // Note: security has not been enabled.
		kv_impl_init();
        get_con_flash(&loop_count);
		check_flash(&loop_count);
		read_rest_flash(&loop_count ,RESET_CHECK);
        find_change_adv();
    
    att_server_init(att_read_callback, att_write_callback);
    hci_event_callback_registration.callback = &user_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);
    att_server_register_packet_handler(&user_packet_handler);
    return 0;
}

