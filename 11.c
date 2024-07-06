/**
 * Copyright (c) 2019, Freqchip
 * 
 * All rights reserved.
 * 
 * 
 */
 
 /*
 * INCLUDES (包含头文件)
 */
#include <stdbool.h>
#include "gap_api.h"
#include "gatt_api.h"
#include "driver_gpio.h"
#include "simple_gatt_service.h"
#include "ble_multi_role.h"

#include "sys_utils.h"
#include "flash_usage_config.h"
/*
 * MACROS (宏定义)
 */

/*
 * CONSTANTS (常量定义)
 */

// GAP - Advertisement data (max size = 31 bytes, though this is
// best kept short to conserve power while advertisting)
// GAP-广播包的内容,最长31个字节.短一点的内容可以节省广播时的系统功耗.
static uint8_t adv_data[] =
{
  // service UUID, to notify central devices what services are included
  // in this peripheral. 告诉central本机有什么服务, 但这里先只放一个主要的.
  0x03,   // length of this data
  GAP_ADVTYPE_16BIT_MORE,      // some of the UUID's, but not all
  0xFF,
  0xFE,
};

// GAP - Scan response data (max size = 31 bytes, though this is
// best kept short to conserve power while advertisting)
// GAP-Scan response内容,最长31个字节.短一点的内容可以节省广播时的系统功耗.
static uint8_t scan_rsp_data[] =
{
  // complete name 设备名字
  0x12,   // length of this data
  GAP_ADVTYPE_LOCAL_NAME_COMPLETE,
  'S','i','m','p','l','e',' ','M','u','l','t','i',' ','R','o','l','e',

  // Tx power level 发射功率
  0x02,   // length of this data
  GAP_ADVTYPE_POWER_LEVEL,
  0,	   // 0dBm
};

#define SP_CHAR1_UUID            0xFFF1
#define SP_CHAR2_UUID            0xFFF2

const gatt_uuid_t client_att_tb[] =
{
    [0]  =
    { UUID_SIZE_2, UUID16_ARR(SP_CHAR1_UUID)},
    [1]  =
    { UUID_SIZE_2, UUID16_ARR(SP_CHAR2_UUID)},
};

/*
 * TYPEDEFS (类型定义)
 */

/*
 * GLOBAL VARIABLES (全局变量)
 */

/*
 * LOCAL VARIABLES (本地变量)
 */
static uint8_t client_id;

//static uint8_t master_link_conidx;
 
/*
 * LOCAL FUNCTIONS (本地函数)
 */
static void mr_start_adv(void);
static void mr_central_start_scan(void);
static uint16_t mr_central_msg_handler(gatt_msg_t *p_msg);
static void mr_peripheral_init(void);
static void mr_central_init(void);
/*
 * EXTERN FUNCTIONS (外部函数)
 */

/*
 * PUBLIC FUNCTIONS (全局函数)
 */


/*********************************************************************
 * @fn      app_gap_evt_cb
 *
 * @brief   Application layer GAP event callback function. Handles GAP evnets.
 *
 * @param   p_event - GAP events from BLE stack.
 *       
 *
 * @return  None.
 */
void app_gap_evt_cb(gap_event_t *p_event)
{
    switch(p_event->type)
    {
        /****************** Slave role events ******************/
        case GAP_EVT_ADV_END:
        {
            co_printf("adv_end,status:0x%02x\r\n",p_event->param.adv_end.status);
            //gap_start_advertising(0);
        }
        break;
        
        case GAP_EVT_ALL_SVC_ADDED:
        {
            mr_start_adv();
#ifdef USER_MEM_API_ENABLE
            //show_mem_list();
            //show_msg_list();
            //show_ke_malloc();
#endif
        }
        break;

        case GAP_EVT_SLAVE_CONNECT:
        {
            co_printf("slave[%d],connect. link_num:%d\r\n",p_event->param.slave_connect.conidx,gap_get_connect_num());
        }
        break;

        case GAP_SEC_EVT_SLAVE_ENCRYPT:
            co_printf("slave[%d]_encrypted\r\n",p_event->param.slave_encrypt_conidx);
            break;
        
        /****************** Master role events ******************/
         case GAP_EVT_SCAN_END:
            co_printf("scan_end,status:0x%02x\r\n",p_event->param.scan_end_status);
            break;
 
        case GAP_EVT_ADV_REPORT:
        {
            uint8_t scan_name[] = "SimpleBLEPeripheral";
            //if(memcmp(event->param.adv_rpt->src_addr.addr.addr,"\x0C\x0C\x0C\x0C\x0C\x0B",6)==0)
            if (p_event->param.adv_rpt->data[0] == 0x14
                && p_event->param.adv_rpt->data[1] == GAP_ADVTYPE_LOCAL_NAME_COMPLETE
                && memcmp(&(p_event->param.adv_rpt->data[2]), scan_name, 0x12) == 0)
            {
                gap_stop_scan();
                
                co_printf("evt_type:0x%02x,rssi:%d\r\n",p_event->param.adv_rpt->evt_type,p_event->param.adv_rpt->rssi);

                co_printf("content:");
                show_reg(p_event->param.adv_rpt->data,p_event->param.adv_rpt->length,1);
                
                gap_start_conn(&(p_event->param.adv_rpt->src_addr.addr),
                                p_event->param.adv_rpt->src_addr.addr_type,
                                12, 12, 0, 300);
            }

        }
        break;

        case GAP_EVT_MASTER_CONNECT:
        {
            co_printf("master[%d],connect. link_num:%d\r\n",p_event->param.master_connect.conidx,gap_get_connect_num());
            //master_link_conidx = (p_event->param.master_connect.conidx);
#if 1
            if (gap_security_get_bond_status())
                gap_security_enc_req(p_event->param.master_connect.conidx);
            else
                gap_security_pairing_req(p_event->param.master_connect.conidx);
#else
            extern uint8_t client_id;
            gatt_discovery_all_peer_svc(client_id,event->param.master_encrypt_conidx);
#endif
            mr_central_start_scan();
        }
        break;
       
        
        /****************** Common events ******************/
        case GAP_EVT_DISCONNECT:
        {
            co_printf("Link[%d] disconnect,reason:0x%02X\r\n",p_event->param.disconnect.conidx
                      ,p_event->param.disconnect.reason);
            mr_start_adv();
#ifdef USER_MEM_API_ENABLE
            show_mem_list();
            //show_msg_list();
            show_ke_malloc();
#endif
        }
        break;
        
        case GAP_EVT_LINK_PARAM_REJECT:
            co_printf("Link[%d]param reject,status:0x%02x\r\n"
                      ,p_event->param.link_reject.conidx,p_event->param.link_reject.status);
            break;

        case GAP_EVT_LINK_PARAM_UPDATE:
            co_printf("Link[%d]param update,interval:%d,latency:%d,timeout:%d\r\n",p_event->param.link_update.conidx
                      ,p_event->param.link_update.con_interval,p_event->param.link_update.con_latency,p_event->param.link_update.sup_to);
            break;

        case GAP_EVT_PEER_FEATURE:
            co_printf("peer[%d] feats ind\r\n",p_event->param.peer_feature.conidx);
            show_reg((uint8_t *)&(p_event->param.peer_feature.features),8,1);
            break;

        case GAP_EVT_MTU:
            co_printf("mtu update,conidx=%d,mtu=%d\r\n"
                      ,p_event->param.mtu.conidx,p_event->param.mtu.value);
            break;
        
        case GAP_EVT_LINK_RSSI:
            co_printf("link rssi %d\r\n",p_event->param.link_rssi);
            break;
        
        default:
            break;
    }
}

/*********************************************************************
 * @fn      sp_start_adv
 *
 * @brief   Set advertising data & scan response & advertising parameters and start advertising
 *
 * @param   None. 
 *       
 *
 * @return  None.
 */
static void mr_start_adv(void)
{
    // Set advertising parameters
    gap_adv_param_t adv_param;
    adv_param.adv_mode = GAP_ADV_MODE_UNDIRECT;
    adv_param.adv_addr_type = GAP_ADDR_TYPE_PUBLIC;
    adv_param.adv_chnl_map = GAP_ADV_CHAN_ALL;
    adv_param.adv_filt_policy = GAP_ADV_ALLOW_SCAN_ANY_CON_ANY;
    adv_param.adv_intv_min = 300;
    adv_param.adv_intv_max = 300;
        
    gap_set_advertising_param(&adv_param);
    
    // Set advertising data & scan response data
	gap_set_advertising_data(adv_data, sizeof(adv_data));
	gap_set_advertising_rsp_data(scan_rsp_data, sizeof(scan_rsp_data));
    // Start advertising
	co_printf("Start advertising...\r\n");
	gap_start_advertising(0);
}

/*********************************************************************
 * @fn      mr_central_start_scan
 *
 * @brief   Set central role scan parameters and start scanning BLE devices.
 *
 * @param   None. 
 *       
 *
 * @return  None.
 */
static void mr_central_start_scan(void)
{
    // Start Scanning
    co_printf("Start scanning...\r\n");
    gap_scan_param_t scan_param;
    scan_param.scan_mode = GAP_SCAN_MODE_GEN_DISC;
    scan_param.dup_filt_pol = 0;
    scan_param.scan_intv = 32;  //scan event on-going time
    scan_param.scan_window = 20;
    scan_param.duration = 0;
    gap_start_scan(&scan_param);
}

/*********************************************************************
 * @fn      mr_central_msg_handler
 *
 * @brief   Multi Role Central GATT message handler, handles messages from GATT layer.
 *          Messages like read/write response, notification/indication values, etc.
 *
 * @param   p_msg       - GATT message structure.
 *
 * @return  uint16_t    - Data length of the GATT message handled.
 */
static uint16_t mr_central_msg_handler(gatt_msg_t *p_msg)
{
    co_printf("CCC:%x\r\n",p_msg->msg_evt);
    switch(p_msg->msg_evt)
    {
        case GATTC_MSG_NTF_REQ:
        {
            if(p_msg->att_idx == 0)
            {
                show_reg(p_msg->param.msg.p_msg_data,p_msg->param.msg.msg_len,1);
            }
        }
        break;
        
        case GATTC_MSG_READ_IND:
        {
            if(p_msg->att_idx == 0)
            {
                show_reg(p_msg->param.msg.p_msg_data,p_msg->param.msg.msg_len,1);
            }
        }
        break;
        
        case GATTC_MSG_CMP_EVT:
        {
            co_printf("op:%d done\r\n",p_msg->param.op.operation);
            if(p_msg->param.op.operation == GATT_OP_PEER_SVC_REGISTERED)
            {
                uint16_t att_handles[2];
                memcpy(att_handles,p_msg->param.op.arg,4);
                show_reg((uint8_t *)att_handles,4,1);

                gatt_client_enable_ntf_t ntf_enable;
                ntf_enable.conidx = p_msg->conn_idx;
                ntf_enable.client_id = client_id;
                ntf_enable.att_idx = 0; //TX
                gatt_client_enable_ntf(ntf_enable);

                gatt_client_write_t write;
                write.conidx = p_msg->conn_idx;
                write.client_id = client_id;
                write.att_idx = 1; //RX
                write.p_data = "\x1\x2\x3\x4\x5\x6\x7";
                write.data_len = 7;
                gatt_client_write_cmd(write);

                gatt_client_read_t read;
                read.conidx = p_msg->conn_idx;
                read.client_id = client_id;
                read.att_idx = 0; //TX
                gatt_client_read(read);
            }
        }
        break;
        
        default:
        break;
    }

    return 0;
}


/*********************************************************************
 * @fn      mr_peripheral_init
 *
 * @brief   Initialize multi role's peripheral profile, BLE related parameters.
 *
 * @param   None. 
 *       
 *
 * @return  None.
 */
static void mr_peripheral_init(void)
{
    // Adding services to database
    sp_gatt_add_service();  
}

/*********************************************************************
 * @fn      mr_central_init
 *
 * @brief   Initialize multi role's central, BLE related parameters.
 *
 * @param   None. 
 *       
 *
 * @return  None.
 */
static void mr_central_init(void)
{
    gatt_client_t client;
    
    client.p_att_tb = client_att_tb;
    client.att_nb = 2;
    client.gatt_msg_handler = mr_central_msg_handler;
    client_id = gatt_add_client(&client);
    
    mr_central_start_scan();
}

/*********************************************************************
 * @fn      multi_role_init
 *
 * @brief   Initialize multi role, including peripheral & central roles, 
 *          and BLE related parameters.
 *
 * @param   None. 
 *       
 *
 * @return  None.
 */
void multi_role_init(void)
{
    // Initialize security related settings.
    gap_security_param_t param =
    {
        .mitm = false,
        .ble_secure_conn = false,
        .io_cap = GAP_IO_CAP_NO_INPUT_NO_OUTPUT,
        .pair_init_mode = GAP_PAIRING_MODE_WAIT_FOR_REQ,
        .bond_auth = true,
        .password = 0,
    };
    
    gap_security_param_init(&param);
    
    gap_set_cb_func(app_gap_evt_cb);

    gap_bond_manager_init(BLE_BONDING_INFO_SAVE_ADDR, BLE_REMOTE_SERVICE_SAVE_ADDR, 8, true);
    gap_bond_manager_delete_all();
    
    // set local device name
    uint8_t local_name[] = "Simple Multi Role";
    gap_set_dev_name(local_name, sizeof(local_name));

    mac_addr_t addr;
    gap_address_get(&addr);
    co_printf("Local BDADDR: 0x%2X%2X%2X%2X%2X%2X\r\n", addr.addr[0], addr.addr[1], addr.addr[2], addr.addr[3], addr.addr[4], addr.addr[5]);
    
    mr_peripheral_init();
    mr_central_init();
}
#define CALORIES_SPEED_RATIO_BIKE  0.0956 // 10km/h => 0.1cal/s for treadmill   40*watt J/s
#define CALORIES_SPEED_RATIO_ELLIP  0.1434//60*watt J/s



#ifdef CROSS_TRAINER
		tDef_Sport_data.Calories.Value += tDef_Sport_data.Watt.Value*CALORIES_SPEED_RATIO_ELLIP;
#else		
		tDef_Sport_data.Calories.Value += tDef_Sport_data.Watt.Value*CALORIES_SPEED_RATIO_BIKE;// Watt*0.004 kj/s   0.4/4.184
#endif		


const uint16_t Watt_table[11][24]={
	{ 8, 9,10,11,12,14,15,16,17,18,19,20,22,23,24,26,27,28,30,31,32,33,35,36},//20rpm  
	{15,18,20,23,25,28,31,33,36,39,41,43,46,50,52,55,58,61,63,66,69,72,74,78},
	{25,29,34,38,42,46,51,56,60,65,68,73,77,83,86,92, 97,   102,106,110,116,120,125,131,},//add 11.08 97
	{ 36, 42, 49, 55, 61, 68, 75, 82, 88, 94,100,107,114,122,128,136,142,150,156,162,171,177,185,192},
	{ 48, 57, 65, 74, 82, 91,100,110,119,127,135,144,153,164,173,182,192,201,211,220,230,241,251,260},
	{ 61, 72, 84, 95,105,117,129,141,153,164,174,185,197,212,221,233,247,260,269,279,295,305,318,328},
	{ 75, 88,102,116,129,143,158,173,189,201,213,227,241,260,272,290,303,319,331,342,361,375,388,401},
	{ 89,105,122,139,154,173,190,207,224,241,255,272,289,311,326,346,362,380,394,412,430,443,458,477},//90rpm
	{104,122,142,161,179,199,219,240,261,281,295,316,335,361,378,401,419,446,459,473,492,514,531,550},//100rpm
	{117,137,159,182,203,227,248,271,297,316,334,357,379,407,427,452,472,500,519,539,565,583,609,630},
	{132,155,179,205,228,255,279,307,334,357,379,397,430,460,480,514,537,564,588,606,637,661,687,715},//120
};



uint16_t Watt_calcu(uint16_t level)
{
	uint8_t RPM_x100;
	uint16_t Watt_lower_value;
	uint16_t Watt_higher_value;
	uint16_t watt_temp;
	if(level> MAX_LEVEL)
	{
		level = MAX_LEVEL;
	}
	
	RPM_x100 = RPM_result/10;
	if(RPM_ready)
	{
		if(RPM_x100 < 2)//RPM 15-19.9;
		{
			Watt_lower_value = Watt_table[0][level-1];//20rpm
			Watt_higher_value = Watt_table[0][level-1];
		}
		else if(RPM_x100 >= 12)//>=120
		{
			Watt_lower_value = Watt_table[10][level-1];//120rpm
			Watt_higher_value = Watt_table[10][level-1];
		}
		else
		{
			Watt_lower_value = Watt_table[RPM_x100-2][level-1];
			Watt_higher_value = Watt_table[RPM_x100-1][level-1];
		}
		watt_temp = (Watt_higher_value - Watt_lower_value)*(RPM_result*10 - RPM_x100*100)/100+ Watt_lower_value;
	}
	else
	{
		watt_temp = 0;
	}
	return watt_temp;
}






uint32_t Level_calcu(uint16_t watt_set)
{
	uint8_t Watt_level_OK;
	uint8_t Calcu_times;
	//int16_t div; //not used
	Level_manual_temp= tDef_Sport_data.Resistance.Value;
	if(Count_watt_3S++>3)
	{
		Count_watt_3S = 0;
		if(RPM_ready == 0)// if no rpm level =1
		{
			Level_manual_temp = 1;
		}
		else
		{
			Watt_level_OK = 0;
			Calcu_times = 0;
			Watt_deviation = 1;
			Watt_result_temp = Watt_calcu(Level_manual_temp);
			Watt_deviation_buff = Watt_result_temp-watt_set;
			Level_manual_last = Level_manual_temp;
			if(Watt_deviation_buff == 0)
			{
				Watt_level_OK = 1;
			}
			else if(Watt_deviation_buff>0)
			{
				Level_manual_temp--;
				if(Level_manual_temp<1)
				{
					Level_manual_temp = 1;
					Watt_level_OK = 1;
				}
			}
			else if(Watt_deviation_buff<0)
			{
				Level_manual_temp++;
				if(Level_manual_temp > MAX_LEVEL)
				{
					Level_manual_temp = MAX_LEVEL;
					Watt_level_OK = 1;
				}
			}
			while( (Watt_level_OK == 0)&&(Calcu_times <MAX_LEVEL) )//modi  240424 24->MAX_LEVEL
			{
				Watt_result_temp = Watt_calcu(Level_manual_temp);
				Watt_deviation_buff2 = Watt_deviation_buff;//buff2 last deviation, buff current deviation
				Watt_deviation_buff = Watt_result_temp-watt_set;
				Calcu_times++;
				if( (Watt_deviation_buff2>0)&&(Watt_deviation_buff<0) )
				{
					if( (Watt_deviation_buff2+ Watt_deviation_buff)>=0 )//more close to current 
					{
						;
					}
					else
					{
						Level_manual_temp = Level_manual_last;
					}
					Watt_level_OK = 1;
				}
				else if(( (Watt_deviation_buff2<0)&&(Watt_deviation_buff>0) ))
				{
					if( (Watt_deviation_buff2+ Watt_deviation_buff)>=0 )//more close to last 
					{
						Level_manual_temp = Level_manual_last;
					}
					else
					{
						;
					}
					Watt_level_OK = 1;
				}
				else if (Watt_deviation_buff2 == 0)
				{
					Level_manual_temp = Level_manual_last;
					Watt_level_OK = 1;
				}					
				else if (Watt_deviation_buff == 0)
				{
					Watt_level_OK = 1;
				}
				else
				{
					if(Watt_deviation_buff > 0)//
					{
						Level_manual_temp--;
						if(Level_manual_temp<1)
						{
							Level_manual_temp = 1;
							Watt_level_OK = 1;
						}
						else
						{
							Level_manual_last = Level_manual_temp;
						}
					}
					else if(Watt_deviation_buff < 0 ) 
					{
						Level_manual_temp++;
						if(Level_manual_temp>MAX_LEVEL)
						{
							Level_manual_temp = MAX_LEVEL;
							Watt_level_OK = 1;
						}
						else
						{
							Level_manual_last = Level_manual_temp;
						}
					}
					else
					{
						Watt_level_OK = 1;
					}
				}
			}
		} 
	}
	return Level_manual_temp;
}
