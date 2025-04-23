#include "kv_flash.h"
#include "kv_storage.h"
#include "stdlib.h"
#include "string.h"
#include "platform_api.h"
#include <stdint.h>

//void check_flash(adv_loop_t * data , kv_key_t key)
//{
//    int16_t length = 0;
//    adv_loop_t *temp_dat = (adv_loop_t *)kv_get(key ,&length);
//    if(temp_dat == NULL || (length != sizeof(adv_loop_t))){
//        kv_put(key ,(const uint8_t *)&data->loop_cnt , sizeof(data->loop_cnt));
//    }
//		else{
//			temp_dat->loop_cnt > 255 ? temp_dat->loop_cnt = 255 : temp_dat->loop_cnt++;
//			kv_value_modified_of_key(key);
//		}
//}

void adv_uint8_to_ascii(adv_loop_t *ustruct)
{
	platform_printf("1. is %d\n",ustruct->loop_cnt);
    ustruct->ascii_cnt[0] = (ustruct->loop_cnt / 100) + '0';
    ustruct->ascii_cnt[1] = ((ustruct->loop_cnt % 100) / 10) + '0';
    ustruct->ascii_cnt[2] = (ustruct->loop_cnt % 10) + '0';

	ustruct->ascii_cnt[3] = (ustruct->conn_cnt % 10) + '0';
}

void check_flash(adv_loop_t * data)
{
	int16_t length = 0;
	adv_loop_t *temp_dat = (adv_loop_t *)kv_get(RESET_CHECK ,&length);
	if(temp_dat == NULL || (length != sizeof(adv_loop_t))){
		kv_put(RESET_CHECK ,(const uint8_t *)data , sizeof(adv_loop_t));
	}
	else{
			temp_dat->loop_cnt > 255 ? temp_dat->loop_cnt = 255 : temp_dat->loop_cnt++;
			adv_uint8_to_ascii(temp_dat);
			kv_value_modified_of_key(RESET_CHECK);
		
		}   
}

uint8_t read_rest_flash(adv_loop_t * data ,kv_key_t key)
{
		int16_t length = 0;
		adv_loop_t *temp_dat = (adv_loop_t *)kv_get(key ,&length);
		if(temp_dat == NULL || (length != sizeof(adv_loop_t))){
			return 1;
		}
		else{
			data->loop_cnt = temp_dat->loop_cnt;
			data->ascii_cnt[0] = temp_dat->ascii_cnt[0];
			data->ascii_cnt[1] = temp_dat->ascii_cnt[1];
			data->ascii_cnt[2] = temp_dat->ascii_cnt[2];
			adv_uint8_to_ascii(data);
			return 0;
		}
}

void get_con_flash(adv_loop_t * data)
{
	int16_t length = 0;
	uint8_t *temp_dat = kv_get(CONNECT_CHECK ,&length);
	if(temp_dat == NULL || (length != sizeof(uint8_t))){
		kv_put(CONNECT_CHECK ,(const uint8_t *)&data->conn_cnt , sizeof(uint8_t));
	}
	else{
			*temp_dat > 9 ? data->conn_cnt = 9 : (data->conn_cnt = *temp_dat);
			platform_printf("temp is %d\n",*temp_dat);
			platform_printf("data is %d\n",data->conn_cnt);
	}
} 

