/*================================================================
  Copyright (c) 2024, Quectel Wireless Solutions Co., Ltd. All rights reserved.
  Quectel Wireless Solutions Proprietary and Confidential.
=================================================================*/
/**************************************************************************************************
* include
**************************************************************************************************/
#include "ql_include.h"

#if CFG_ENABLE_QUECTEL_BLE_PERIPHERA || CFG_ENABLE_QUECTEL_BLE_CENTRAL
/**************************************************************************************************
* define
**************************************************************************************************/
#define QL_BLE_DEMO_QUEUE_MAX_NB      10

#if CFG_ENABLE_QUECTEL_BLE_PERIPHERA
#define QL_BLE_DEMO_DFLT_ADV_NAME     "FC41D_DEMO"
#define QL_BLE_DEMO_DFLT_ADV_NAME_LEN 10

#define QL_DEMO_ATT_DECL_PRIMARY_SERVICE     {0x00,0x28,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
#define QL_DEMO_ATT_DECL_CHARACTERISTIC      {0x03,0x28,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
#define QL_DEMO_ATT_DESC_CLIENT_CHAR_CFG     {0x02,0x29,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
#define QL_BLE_TEST_CHARACTERISTIC           {0x15,0xFF,0,0,0x34,0x56,0,0,0,0,0x28,0x37,0,0,0,0}

#define QUEC_IBEACON_ENABLE 0
#endif
/**************************************************************************************************
* param
**************************************************************************************************/
#if CFG_ENABLE_QUECTEL_BLE_PERIPHERA
enum
{
	QL_BLE_DEMO_IDX_SVC,
	QL_BLE_DEMO_IDX_TEST_VAL_CHAR,
	QL_BLE_DEMO_IDX_TEST_VAL_VALUE,
	QL_BLE_DEMO_IDX_TEST_VAL_NTF_CFG,
	QL_BLE_DEMO_IDX_NB,
};
    
ql_attm_desc_t ql_ble_demo_att_db[QL_BLE_DEMO_IDX_NB] =
{
	//  Service Declaration
	[QL_BLE_DEMO_IDX_SVC]              = {QL_DEMO_ATT_DECL_PRIMARY_SERVICE, QL_PERM_SET(RD, ENABLE), 0, 0},

    [QL_BLE_DEMO_IDX_TEST_VAL_CHAR]    = {QL_DEMO_ATT_DECL_CHARACTERISTIC , QL_PERM_SET(RD, ENABLE), 0, 0},
	[QL_BLE_DEMO_IDX_TEST_VAL_VALUE]   = {QL_BLE_TEST_CHARACTERISTIC      , QL_PERM_SET(WRITE_REQ, ENABLE)|QL_PERM_SET(NTF, ENABLE), QL_PERM_SET(RI, ENABLE)|QL_PERM_SET(UUID_LEN, UUID_128), 512},
	[QL_BLE_DEMO_IDX_TEST_VAL_NTF_CFG] = {QL_DEMO_ATT_DESC_CLIENT_CHAR_CFG, QL_PERM_SET(RD, ENABLE)|QL_PERM_SET(WRITE_REQ, ENABLE), 0, 0},
};
#endif

typedef enum
{
    QL_BLE_DEMO_START_ADV,
    QL_BLE_DEMO_PER_GATT_SEND_NTF,
    QL_BLE_DEMO_START_SCAN,
    QL_BLE_DEMO_STOP_SCAN,    
    QL_BLE_DEMO_CREATE_CONN,
    QL_BLE_DEMO_CONN_PEER_DEVICE,
    QL_BLE_DEMO_CENTRAL_EXCHANGE_MTU,
    QL_BLE_DEMO_CEN_GATT_NTFCFG,
    QL_BLE_DEMO_CEN_GATT_WRCMD,
    QL_BLE_DEMO_CEN_GATT_WRREQ,
} ql_ble_demo_msg_id;

typedef struct
{
    uint32_t msg_id;
    
    union
    {
        struct
        {       
            uint8_t con_idx;
            uint8_t prf_id;
            uint8_t att_idx; 
            uint8_t *data;
            uint32_t len;
        }send_ntf;

        struct
        {       
            uint8_t conn_idx;
            uint16_t chara_handle;
            uint8_t *data;
            uint16_t data_len;
        }wr;
    }msg_param;
}ql_ble_demo_msg;

static uint8_t adv_actv_idx     = 0xFF;
static uint8_t ibc_adv_actv_idx = 0xFF;
static uint8_t scan_actv_idx    = 0xFF;
static uint8_t conn_actv_idx    = 0xFF;
static uint8_t conn_idx         = 0;
static uint16_t chara_handle    = 0;
static uint8_t conn_peer_addr_type      = 0; 
static struct ql_bd_addr conn_peer_addr = {0};

static ql_sem_t   ql_ble_demo_sem  = NULL;
static ql_task_t  ql_ble_demo_task = NULL;
static ql_queue_t ql_ble_demo_q    = NULL;
/**************************************************************************************************
* Description:
*	Sending BLE demo messages.
* 
* Parameters:
*	ble_msg
*	
* Return:
*	none
*	
**************************************************************************************************/
void ql_ble_demo_msg_send(ql_ble_demo_msg *ble_msg)
{
    if(ql_rtos_queue_release(ql_ble_demo_q, sizeof(ql_ble_demo_msg), (uint8_t *)ble_msg, QL_NO_WAIT) != 0)
    {
        os_printf("BLE: %s, error \r\n", __func__);
    }
}

/**************************************************************************************************
* Description:
*	Execute BLE command function callback function.
* 
* Parameters:
*	cmd
*	param
*	
* Return:
*	none
*	
**************************************************************************************************/
void ql_ble_demo_cmd_cb(ble_cmd_t cmd, ble_cmd_param_t *param)
{
	bk_printf("BLE: cmd:%d, status:%d\r\n", cmd, param->status);
    
    if((cmd == BLE_INIT_CREATE) && (param->status == 0))  
    {
        ql_ble_demo_msg ble_demo_msg = {0};
        ble_demo_msg.msg_id = QL_BLE_DEMO_CONN_PEER_DEVICE;
        ql_ble_demo_msg_send(&ble_demo_msg);
    }
    else
    if((cmd == BLE_STOP_SCAN) && (param->status == 0))
    {
        ql_ble_demo_msg ble_demo_msg = {0};
        ble_demo_msg.msg_id = QL_BLE_DEMO_CREATE_CONN;
        ql_ble_demo_msg_send(&ble_demo_msg);
    }
}

#if CFG_ENABLE_QUECTEL_BLE_PERIPHERA
/**************************************************************************************************
* Description:
*	ble legacy advertise.
* 
* Parameters:
*	none
*	
* Return:
*	none
*	
**************************************************************************************************/
static void ql_ble_demo_legacy_adv(void)
{
    ql_errcode_bt_e ret = 0;
    struct adv_param adv_p = {0};
    adv_p.interval_min = 160;
    adv_p.interval_max = 160;
    adv_p.channel_map = 7;
    adv_p.duration = 0;
    adv_p.advData[0] = 0x02;
    adv_p.advData[1] = 0x01;
    adv_p.advData[2] = 0x06;
    adv_p.advData[3] = QL_BLE_DEMO_DFLT_ADV_NAME_LEN + 1;
    adv_p.advData[4] = 0x09;
    memcpy(&adv_p.advData[5], QL_BLE_DEMO_DFLT_ADV_NAME, QL_BLE_DEMO_DFLT_ADV_NAME_LEN);
    adv_p.advDataLen = 5 + QL_BLE_DEMO_DFLT_ADV_NAME_LEN;

    if(adv_actv_idx == 0xFF)
    {
        os_printf("adv_actv_idx has not been created yet\r\n");
        
        adv_actv_idx = ql_ble_get_idle_actv_idx_handle();
        if (adv_actv_idx >= BLE_ACTIVITY_MAX)
        {
            os_printf("BLE: Failed to obtain the broadcast link\r\n");
            return ;
        }
        /* If this actv_ Idx is used for the first time , create it */
        if (app_ble_actv_state_get(adv_actv_idx) == ACTV_IDLE)
        {
            ret = ql_ble_adv_start(adv_actv_idx, &adv_p, ql_ble_demo_cmd_cb);
        }
    }
    else
    {
        ret = ql_ble_start_advertising(adv_actv_idx, 0, ql_ble_demo_cmd_cb);
    }
    
    if(ret != QL_BT_SUCCESS)
    {
        os_printf("BLE: Failed to start broadcasting, ret: %d\r\n", ret);
    }
    else
    {
        os_printf("BLE: Broadcasting started successfully \r\n");
    }
}

#if QUEC_IBEACON_ENABLE
/**************************************************************************************************
* Description:
*	start ble ibeacon advertise.
* 
* Parameters:
*	none
*	
* Return:
*	none
*	
**************************************************************************************************/
void ql_ble_demo_start_ibeacon_adv(void)
{    
    ql_errcode_bt_e ret = 0;
    struct adv_param adv_ibc = {0};
    uint8_t ibeacon_adv_data[] =  
    {        
       0x02,0x01,0x06,        
       0x1A, //length        
       0xFF, //        
       0x4C,0x00, //company : apple,inc.        
       0x02, //type : beacon        
       0x15, // length of data:21bytes        
       0xe2,0xc5,0x6d,0xb5,0xdf,0xfb,0x48,0xd2,0xb0,0x60,0xd0,0xf5,0xa7,0x10,0x96,0xe0, //uuid        
       0x00,0x00, //major        
       0x00,0x00, //minor        
       0xcc, //rssi at 1m    
    };   
       
    adv_ibc.interval_min = 160;    
    adv_ibc.interval_max = 160;    
    adv_ibc.channel_map = 7;    
    adv_ibc.duration = 0;    
    adv_ibc.mode = QL_BLE_ADV_MODE_NON_CONN_SCAN;    
    /******************* *********************/    
    memcpy(adv_ibc.advData , ibeacon_adv_data , sizeof(ibeacon_adv_data));    
    adv_ibc.advDataLen = sizeof(ibeacon_adv_data);
    
    if(ibc_adv_actv_idx == 0xFF)    
    {      
        os_printf("ibc_adv_actv_idx has not been created yet\r\n");
        /* Get idle actv_idx */      
        ibc_adv_actv_idx = ql_ble_get_idle_actv_idx_handle();      
        if (ibc_adv_actv_idx >= BLE_ACTIVITY_MAX)      
        {    
            os_printf("BLE: Failed to obtain the ibeacon broadcast link\r\n");
            return;      
        }      
        /* If this actv_ Idx is used for the first time , create it */      
        if (app_ble_actv_state_get(ibc_adv_actv_idx) == ACTV_IDLE)      
        {          
            ret = ql_ble_adv_start(ibc_adv_actv_idx, &adv_ibc, ql_ble_demo_cmd_cb);  
        }   
    }  
    else
    {
        ret = ql_ble_start_advertising(ibc_adv_actv_idx, 0, ql_ble_demo_cmd_cb);
    }
    
    if(ret != QL_BT_SUCCESS)
    {
        os_printf("BLE: Failed to start ibeacon broadcasting, ret: %d\r\n", ret);
    }
    else
    {
        os_printf("BLE: ibeacon broadcasting started successfully \r\n");
    }
}

/**************************************************************************************************
* Description:
*	stop ble ibeacon advertise.
* 
* Parameters:
*	none
*	
* Return:
*	none
*	
**************************************************************************************************/
void ql_ble_demo_stop_ibeacon_adv(void)
{  
    ql_ble_stop_advertising(ibc_adv_actv_idx, ql_ble_demo_cmd_cb);
}
#endif

/**************************************************************************************************
* Description:
*	ble create profile.
* 
* Parameters:
*	none
*	
* Return:
*	none
*	
**************************************************************************************************/
static void ql_ble_demo_set_profile_and_adv(void)
{
    uint8_t ret = 0;
    uint8_t svc_uuid[16] = {0x36,0xF5,0,0,0x34,0x56,0,0,0,0,0x28,0x37,0,0,0,0};
    
    /* Create BLE service */
    struct ql_ble_db_cfg ble_db_cfg = {0};
    ble_db_cfg.att_db = ql_ble_demo_att_db;
    ble_db_cfg.att_db_nb = QL_BLE_DEMO_IDX_NB;
    ble_db_cfg.prf_task_id = 0;
    ble_db_cfg.start_hdl = 0;
    ble_db_cfg.svc_perm = QL_PERM_SET(SVC_UUID_LEN, UUID_128);
    memcpy(ble_db_cfg.uuid, svc_uuid, 16);
    ret = ql_ble_create_db(&ble_db_cfg);
    if(ret != QL_BT_SUCCESS)
    {
        os_printf("BLE: Failed to create ble service, ret: %d \r\n", ret);
    }
    else
    {
        os_printf("BLE: Create ble service successfully \r\n");
    }
    
    ql_ble_demo_legacy_adv();
    #if QUEC_IBEACON_ENABLE
    ql_ble_demo_start_ibeacon_adv();
    #endif
}
#endif

#if CFG_ENABLE_QUECTEL_BLE_CENTRAL
/**************************************************************************************************
* Description:
*	ble start scan.
* 
* Parameters:
*	none
*	
* Return:
*	none
*	
**************************************************************************************************/
static void ql_ble_demo_start_scan(void)
{
    ql_errcode_bt_e ret = 0;
    struct scan_param scan_info = {0};
        
    /* Set scan param */
    scan_info.channel_map = 0x07;
    scan_info.interval    = 0x50;
    scan_info.window      = 0x20;
    
    if(scan_actv_idx == 0xFF)
    {
        os_printf("scan_actv_idx has not been created yet\r\n");

        scan_actv_idx = ql_ble_get_idle_actv_idx_handle();
        if (scan_actv_idx >= BLE_ACTIVITY_MAX)
        {
            os_printf("BLE: Failed to obtain scan index \r\n");
            return;
        }
        
        /* If this actv_Idx is used for the first time . Start after creating it*/
        if (app_ble_actv_state_get(scan_actv_idx) == ACTV_IDLE)
        {
            ret = ql_ble_scan_start(scan_actv_idx, &scan_info, ql_ble_demo_cmd_cb);
        }
    }
    else
    {
        ret = ql_ble_start_scaning(scan_actv_idx, scan_info.interval, scan_info.window, ql_ble_demo_cmd_cb);
    }
    
    if(ret != QL_BT_SUCCESS)
    {
        os_printf("BLE: Failed to start BLE scanning, ret: %d \r\n", ret);
    }
    else
    {
        os_printf("BLE: BLE scanning is successfully started \r\n");
    }
}

/* Definition of an AD Structure as contained in Advertising and Scan Response packets.
   An Advertising or Scan Response packet contains several AD Structures. 
 */
typedef struct gapAdStructure_tag
{
    uint8_t  length;     /*!< Total length of the [adType + aData] fields. Equal to 1 + lengthOf(aData). */
    uint8_t  adType;     /*!< AD Type of this AD Structure. */
    uint8_t* aData;      /*!< Data contained in this AD Structure; length of this array is equal to (gapAdStructure_t.length - 1). */
} gapAdStructure_t;

void ql_ble_demo_deal_report_adv(const recv_adv_t *p_adv_report)
{
    uint8_t index = 0;
  
    while(index < p_adv_report->data_len)
    {
        gapAdStructure_t adElement = {0};
       
        adElement.length = p_adv_report->data[index];
        adElement.adType = p_adv_report->data[index + 1];
        adElement.aData = (uint8_t *)&p_adv_report->data[index + 2];
    
        if(!memcmp(adElement.aData , "phone_server" , strlen("phone_server")))  //check adv name
        {            
            os_printf("Found Device NAME, RSSI:%ddBm, mac:", p_adv_report->rssi);
            for(uint8_t i = 0; i < 6; i++)
            {
                os_printf("%02X ", p_adv_report->adv_addr[i]);
            }
            os_printf("\r\n");

            memcpy(&conn_peer_addr.addr[0], p_adv_report->adv_addr, 6);
            conn_peer_addr_type = p_adv_report->adv_addr_type;

            ql_ble_demo_msg ble_demo_msg = {0};
            ble_demo_msg.msg_id = QL_BLE_DEMO_STOP_SCAN;
            ql_ble_demo_msg_send(&ble_demo_msg);
            return;
        }
        
        /* Move on to the next AD elemnt type */
        index += adElement.length + sizeof(uint8_t);
    }  
}

static void ql_ble_demo_sdp_characteristic_cb(unsigned char conidx, uint16_t chars_val_hdl, unsigned char uuid_len, unsigned char *uuid)
{
    uint8_t temp_uuid[16] = {0};
    uint8_t demo_comm_uuid[2] = {0xFF, 0x01};
    
    os_printf("Peer device characteristic conidx: %d, handle: 0x%02x, uuid_len: %d, UUID: 0x", conidx, chars_val_hdl, uuid_len);
    for(int i = 0; i < uuid_len; i++)
    {
        temp_uuid[i] = uuid[uuid_len-1-i];
        os_printf("%02x ", uuid[i]);
    }
    os_printf("\r\n");

    if((uuid_len == 2) && !os_memcmp((const void *)demo_comm_uuid, (const void *)temp_uuid, 2))
    {
        chara_handle = chars_val_hdl;
        conn_idx     = conidx;        
        ql_ble_demo_msg ble_demo_msg = {0};
        ble_demo_msg.msg_id = QL_BLE_DEMO_CEN_GATT_NTFCFG;
        ql_ble_demo_msg_send(&ble_demo_msg);
    }
}

//receive slave data
void quec_app_sdp_charac_cb(CHAR_TYPE type, uint8 conidx, uint16_t hdl, uint16_t len, uint8 *data)
{
    os_printf("BLE: %s, type:%x \r\n", __func__, type);
    
    switch(type)
    {
        case CHARAC_NOTIFY:
        case CHARAC_INDICATE:
        {
            os_printf("BLE: conidx:%d, handle:%d, len:%d, 0x", conidx, hdl, len);
            for(int i = 0; i< len; i++)
            {
                os_printf("%02x ",data[i]);
            }
            os_printf("\r\n");
            if(hdl == chara_handle)
            {
                ql_ble_demo_msg ble_demo_msg = {0};
                ble_demo_msg.msg_id = QL_BLE_DEMO_CEN_GATT_WRREQ;
                ble_demo_msg.msg_param.wr.conn_idx     = conidx;
                ble_demo_msg.msg_param.wr.chara_handle = hdl;
                ble_demo_msg.msg_param.wr.data_len     = len;
                if((ble_demo_msg.msg_param.wr.data = (uint8_t *)malloc(len+1)) == NULL)
                {
                    os_printf("BLE: %d, no memory for receive buffer. \r\n", __LINE__);
                    return;
                }
                memset((void *)ble_demo_msg.msg_param.wr.data, 0x00, len+1);
                memcpy((void *)ble_demo_msg.msg_param.wr.data, (const void *)data, len);
                ble_demo_msg.msg_param.wr.data[len] = '\0';
                ql_ble_demo_msg_send(&ble_demo_msg);
            }
            break;
        }

        case CHARAC_READ:
        {
            break;
        }
        
        case CHARAC_READ_DONE:
        {
            break;
        }
        
        case CHARAC_WRITE_DONE:
        {
            break;
        }

        default:
            break;
    }
}
#endif

/**************************************************************************************************
* Description:
*	ble message processing.
* 
* Parameters:
*	none
*	
* Return:
*	none
*	
**************************************************************************************************/
static void ql_ble_demo_msg_process(ql_ble_demo_msg *ble_msg)
{
    ql_errcode_bt_e ret = 0;

    switch (ble_msg->msg_id)
    {        
        case QL_BLE_DEMO_START_ADV:
        {
            #if CFG_ENABLE_QUECTEL_BLE_PERIPHERA
            ql_ble_demo_legacy_adv();
            #endif
            break;
        }
        
        case QL_BLE_DEMO_PER_GATT_SEND_NTF:
        {
            ql_ble_gatts_send_ntf_value(ble_msg->msg_param.send_ntf.len, 
                                        ble_msg->msg_param.send_ntf.data,
                                        ble_msg->msg_param.send_ntf.prf_id, 
                                        ble_msg->msg_param.send_ntf.att_idx);
            
            if(ble_msg->msg_param.send_ntf.data != NULL)
            {
                free(ble_msg->msg_param.send_ntf.data);
                ble_msg->msg_param.send_ntf.data = NULL;
            }
            break;
        }
            
        case QL_BLE_DEMO_START_SCAN:
        {
            #if CFG_ENABLE_QUECTEL_BLE_CENTRAL
            ql_ble_demo_start_scan();
            #endif
            break;
        }

        case QL_BLE_DEMO_STOP_SCAN:
        {
            if(ql_ble_stop_scaning(scan_actv_idx, ql_ble_demo_cmd_cb) != QL_BT_SUCCESS)
            {
                os_printf("BLE: Turn off BLE scan failure \r\n");
            }
            else
            {
                os_printf("BLE: Close BLE scan success \r\n");
            }
            
            break;
        }

        case QL_BLE_DEMO_CREATE_CONN:
        {         
            if(conn_actv_idx == 0xFF)
            {
                os_printf("conn_actv_idx has not been created yet\r\n");

                conn_actv_idx = ql_ble_get_idle_conn_idx_handle();
                if (conn_actv_idx >= BLE_ACTIVITY_MAX)
                {
                    os_printf("Unable to get the index of the connection\r\n");
                    return;
                }
                /* actv_Idx is used for the first time , create it */
                ql_ble_create_conn(conn_actv_idx, 0X30, 0, 500, ql_ble_demo_cmd_cb);
            }
            else
            {                
                ql_ble_set_connect_dev_addr(conn_actv_idx, &conn_peer_addr, conn_peer_addr_type); 
                ret = ql_ble_start_conn(conn_actv_idx, ql_ble_demo_cmd_cb);
                if(ret != QL_BT_SUCCESS)
                {
                    os_printf("BLE: Failed to connect to the peer device, ret: %d\r\n", ret);
                }
                else
                {
                    os_printf("BLE: Connect to peer device successfully\r\n");
                }
            }

            break;
        }

        case QL_BLE_DEMO_CONN_PEER_DEVICE:
        {       
            ql_ble_set_connect_dev_addr(conn_actv_idx, &conn_peer_addr, conn_peer_addr_type); 
            ret = ql_ble_start_conn(conn_actv_idx, ql_ble_demo_cmd_cb);
            if(ret != QL_BT_SUCCESS)
            {
                os_printf("BLE: Failed to connect to the peer device, ret: %d\r\n", ret);
            }
            else
            {
                os_printf("BLE: Connect to peer device successfully\r\n");
            }
                        
            break;
        }

        case QL_BLE_DEMO_CENTRAL_EXCHANGE_MTU:
        {
            break;
        }

        case QL_BLE_DEMO_CEN_GATT_NTFCFG:
        {            
            os_printf("BLE: ntf_enable, conn_handle: [%d], chara_handle: [%d] \r\n", conn_idx, chara_handle);
            ql_ble_gattc_ntf_enable(conn_idx, chara_handle+1, true);

            break;
        }
        
        case QL_BLE_DEMO_CEN_GATT_WRREQ:
        {            
            ql_ble_gattc_write_service_data_req(ble_msg->msg_param.wr.conn_idx,
                                                ble_msg->msg_param.wr.chara_handle, 
                                                ble_msg->msg_param.wr.data_len, 
                                                ble_msg->msg_param.wr.data,
                                                NULL);

            if(ble_msg->msg_param.wr.data != NULL)
            {
                free(ble_msg->msg_param.wr.data);
                ble_msg->msg_param.wr.data = NULL;
            }
            
            break;
        }
                
        default:
            break;
    }
}

/**************************************************************************************************
* Description:
*	ble demo notice cb.
* 
* Parameters:
*	ble_msg
*	
* Return:
*	none
*	
**************************************************************************************************/
static void ql_ble_demo_notice_cb(ble_notice_t notice, void *param)
{
    switch (notice)
    {
        case BLE_5_STACK_OK:
        {
            os_printf("ble stack ok\r\n");
            break;
        }
               
        case BLE_5_WRITE_EVENT:
        {
            write_req_t *w_req = (write_req_t *)param;
            
            os_printf("write event: conn_idx:%d, prf_id:%d, att_idx:%d, len:%d, data[0]:%s\r\n",
                      w_req->conn_idx, w_req->prf_id, w_req->att_idx, w_req->len, w_req->value);

            #if CFG_ENABLE_QUECTEL_BLE_PERIPHERA
            ql_ble_demo_msg ble_demo_msg = {0};
            ble_demo_msg.msg_id = QL_BLE_DEMO_PER_GATT_SEND_NTF;
            ble_demo_msg.msg_param.send_ntf.con_idx = w_req->conn_idx;
            ble_demo_msg.msg_param.send_ntf.prf_id  = w_req->prf_id;
            ble_demo_msg.msg_param.send_ntf.att_idx = w_req->att_idx;
            ble_demo_msg.msg_param.send_ntf.len     = w_req->len;
            if((ble_demo_msg.msg_param.send_ntf.data = (uint8_t *)malloc(w_req->len+1)) == NULL)
            {
                os_printf("BLE: %d, no memory for receive buffer. \r\n", __LINE__);
                return;
            }
            memset((void *)ble_demo_msg.msg_param.send_ntf.data, 0x00, w_req->len+1);
            memcpy((void *)ble_demo_msg.msg_param.send_ntf.data, (const void *)w_req->value, w_req->len);
            ble_demo_msg.msg_param.send_ntf.data[w_req->len] = '\0';
            ql_ble_demo_msg_send(&ble_demo_msg);
            #endif

            break;
        }
        
        case BLE_5_READ_EVENT:
        {
            read_req_t *r_req = (read_req_t *)param;
            os_printf("read_cb: conn_idx: %d, prf_id: %d, add_id: %d\r\n",r_req->conn_idx, r_req->prf_id, r_req->att_idx);
            r_req->value[0] = 0x12;
            r_req->value[1] = 0x34;
            r_req->length = 2;
            break;
        }
        
        case BLE_5_REPORT_ADV:
        {
            recv_adv_t *r_ind = (recv_adv_t *)param;
            int8_t rssi = r_ind->rssi;
            os_printf("r_ind: actv_idx:%d, adv_addr:%02x:%02x:%02x:%02x:%02x:%02x, rssi:%d\r\n",
                      r_ind->actv_idx, r_ind->adv_addr[0], r_ind->adv_addr[1], r_ind->adv_addr[2],
                      r_ind->adv_addr[3], r_ind->adv_addr[4], r_ind->adv_addr[5], rssi);
            
            #if CFG_ENABLE_QUECTEL_BLE_CENTRAL
            ql_ble_demo_deal_report_adv(r_ind);
            #endif

            break;
        }
        
        case BLE_5_MTU_CHANGE:
        {
            mtu_change_t *m_ind = (mtu_change_t *)param;
            os_printf("m_ind:conn_idx:%d, mtu_size:%d\r\n", m_ind->conn_idx, m_ind->mtu_size);
            break;
        }
        
        case BLE_5_CONNECT_EVENT:
        {
            conn_ind_t *c_ind = (conn_ind_t *)param;
            os_printf("BLE_5_CONNECT_EVENT c_ind:conn_idx:%d, addr_type:%d, peer_addr:%02x:%02x:%02x:%02x:%02x:%02x\r\n",
                      c_ind->conn_idx, c_ind->peer_addr_type, c_ind->peer_addr[0], c_ind->peer_addr[1],
                      c_ind->peer_addr[2], c_ind->peer_addr[3], c_ind->peer_addr[4], c_ind->peer_addr[5]);

            /* If you need to set the MTU, please uncomment the following code */
//            ql_ble_gatt_mtu_changes(c_ind->conn_idx, 512, NULL); // Set the MTU size to 512

            break;
        }
        
        case BLE_5_DISCONNECT_EVENT:
        {
            discon_ind_t *d_ind = (discon_ind_t *)param;
            os_printf("BLE_5_DISCONNECT_EVENT, conn_idx:%d, reason:%d \r\n", d_ind->conn_idx, d_ind->reason);

            #if CFG_ENABLE_QUECTEL_BLE_PERIPHERA
            ql_ble_demo_msg ble_demo_msg = {0};
            ble_demo_msg.msg_id = QL_BLE_DEMO_START_ADV;
            ql_ble_demo_msg_send(&ble_demo_msg);
            
            #endif
            break;
        }
        
        case BLE_5_ATT_INFO_REQ:
        {
            att_info_req_t *a_ind = (att_info_req_t *)param;
            os_printf("a_ind:conn_idx:%d\r\n", a_ind->conn_idx);
            a_ind->length = 128;
            a_ind->status = ERR_SUCCESS;
            break;
        }
        
        case BLE_5_CREATE_DB:
        {
            create_db_t *cd_ind = (create_db_t *)param;
            os_printf("cd_ind:prf_id:%d, status:%d\r\n", cd_ind->prf_id, cd_ind->status);
            break;
        }
        
        case BLE_5_INIT_CONNECT_EVENT:
        {
            conn_ind_t *c_ind = (conn_ind_t *)param;
            os_printf("BLE_5_INIT_CONNECT_EVENT:conn_idx:%d, addr_type:%d, peer_addr:%02x:%02x:%02x:%02x:%02x:%02x\r\n",
                      c_ind->conn_idx, c_ind->peer_addr_type, c_ind->peer_addr[0], c_ind->peer_addr[1],
                      c_ind->peer_addr[2], c_ind->peer_addr[3], c_ind->peer_addr[4], c_ind->peer_addr[5]);

            /* If you need to set the MTU, please uncomment the following code */
//            ql_ble_gatt_mtu_changes(c_ind->conn_idx, 512, NULL); // Set the MTU size to 512

            #if CFG_ENABLE_QUECTEL_BLE_CENTRAL
            ql_ble_gattc_all_service_discovery(c_ind->conn_idx);
            #endif
            break;
        }
                
        case BLE_5_INIT_DISCONNECT_EVENT:
        {
            discon_ind_t *d_ind = (discon_ind_t *)param;
            os_printf("BLE_5_INIT_DISCONNECT_EVENT, conn_idx:%d, reason:%d\r\n", d_ind->conn_idx, d_ind->reason);
            
            #if CFG_ENABLE_QUECTEL_BLE_CENTRAL
            ql_ble_demo_msg ble_demo_msg = {0};
            ble_demo_msg.msg_id = QL_BLE_DEMO_START_SCAN;
            ql_ble_demo_msg_send(&ble_demo_msg);
            #endif
            break;
        }
        
        case BLE_5_SDP_REGISTER_FAILED:
            os_printf("BLE_5_SDP_REGISTER_FAILED\r\n");
            break;
        
        default:
            break;
    }
}

/**************************************************************************************************
* Description:
*	ble demo task.
* 
* Parameters:
*	arg
*	
* Return:
*	none
*	
**************************************************************************************************/
static void ql_ble_demo_entry(void *arg)
{    
    ql_ble_demo_msg ble_demo_msg;
    uint8_t ble_mac[6] = {0};

    /* Wait BLE stack initialization complete */
     while (kernel_state_get(TASK_BLE_APP) != APPM_READY)
     {
         ql_rtos_task_sleep_ms(200);
     }
     
     ql_ble_address_get(ble_mac);
     os_printf("ble_mac: [%02x:%02x:%02x:%02x:%02x:%02x] \r\n", 
        ble_mac[0], ble_mac[1], ble_mac[2], ble_mac[3], ble_mac[4], ble_mac[5]);

     /* Set ble event callback */
     ql_ble_set_notice_cb(ql_ble_demo_notice_cb);

     #if CFG_ENABLE_QUECTEL_BLE_PERIPHERA
     ql_ble_demo_set_profile_and_adv();
     #elif CFG_ENABLE_QUECTEL_BLE_CENTRAL
     ql_ble_gattc_register_discovery_callback(ql_ble_demo_sdp_characteristic_cb);
     ql_ble_gattc_register_event_recv_callback(quec_app_sdp_charac_cb);
     ql_ble_demo_start_scan();
     #endif
             
    while (1)
    {
        os_memset((void *)&ble_demo_msg, 0x00, sizeof(ql_ble_demo_msg));
        
        if (ql_rtos_queue_wait(ql_ble_demo_q, (uint8 *)&ble_demo_msg, sizeof(ble_demo_msg), QL_WAIT_FOREVER) == 0)
        {
            ql_ble_demo_msg_process(&ble_demo_msg);
        }
    }
}

/**************************************************************************************************
* Description:
*	Create ble demo task.
* 
* Parameters:
*	none
*	
* Return:
*	none
*	
**************************************************************************************************/
void ql_ble_demo_thread_creat(void)
{
    QlOSStatus ret = 0;

    ret = ql_rtos_semaphore_create(&ql_ble_demo_sem, 1);
	if(ret != 0)
    {
		os_printf("Failed to Create BLE semaphore \r\n");
		return;
	}

    ret = ql_rtos_queue_create(&ql_ble_demo_q, sizeof(ql_ble_demo_msg),  QL_BLE_DEMO_QUEUE_MAX_NB);
	if(ret != 0)
    {
		os_printf("Failed to Create BLE msg queue \r\n");
		return;
	}
    
    ret = ql_rtos_task_create(&ql_ble_demo_task,
                              2048,
                              THD_EXTENDED_APP_PRIORITY,
                              "ble_demo",
                              ql_ble_demo_entry,
                              0);

    if (ret != 0)
    {
        os_printf("Error: Failed to create ble task, ret: [%d] \r\n", ret);
        
        if(ql_ble_demo_sem != NULL)
         {
            ql_rtos_semaphore_delete(ql_ble_demo_sem);
        }
        
        if (ql_ble_demo_q != NULL)
        {
            ql_rtos_queue_delete(ql_ble_demo_q);
            ql_ble_demo_q = NULL;
        }
        
        if (ql_ble_demo_task != NULL)
        {
            ql_rtos_task_delete(ql_ble_demo_task);
            ql_ble_demo_task = NULL;
        }
    }
}
#endif
