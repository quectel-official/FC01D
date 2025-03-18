
#include "include.h"
#include "rtos_pub.h"
#include "error.h"
#include "uart_pub.h"
#include "drv_model_pub.h"
#include "quectel_demo_config.h"
#include "BkDriverFlash.h"
#include "ql_uart.h"
#include "ql_flash.h"
#include "ql_api_osi.h"

#if CFG_ENABLE_QUECTEL_UART_UPGRADE

#define SOH 0x01  
#define STX 0x02  
#define EOT 0x04  
#define ACK 0x06  
#define NAK 0x15  


#define PACKET_STX_DATA_SIZE      1024
#define PACKET_SOH_DATA_SIZE      128
#define PACKET_STX_SEZE      1029
#define PACKET_SOH_SEZE      133
                                

#define OTA_START_ADDR 0x132000
#define OTA_PARTITION_SIZE    0xAE000  

#define RECV_FILENAME 0
#define TRANS_DATA 1
#define TRANS_END_EOT1  2
#define TRANS_END_EOT2  3

#define RECV_HEAD 1
#define RECV_NUMBER 2
#define RECV_NON_NUMBER  3
#define RECV_DATA  4
#define RECEIVE_MAX_TIMES 30

unsigned char sequence_num=1;
static unsigned int receiving_progress=0;
static unsigned int total_progress=0;
static   int state=RECV_FILENAME;

static void SEND_ACK()
{
   unsigned char ch=ACK;
    ql_uart_write(QL_UART_PORT_1,&ch,1);
}

static void SEND_NACK()
{
   unsigned char ch=NAK;
   ql_uart_write(QL_UART_PORT_1,&ch,1);
   ql_uart_log("NACK SEND\r\n");
}

static void SEND_C()
{
   ql_uart_write(QL_UART_PORT_1,"C",1);
   ql_uart_log("C SEND\r\n");
}


//uart init
static void uart_init(void)
{

   ql_uart_config_s uart_test_cfg;
   uart_test_cfg.baudrate = QL_UART_BAUD_921600;
   uart_test_cfg.data_bit = QL_UART_DATABIT_8;
   uart_test_cfg.parity_bit = QL_UART_PARITY_NONE;
   uart_test_cfg.stop_bit = QL_UART_STOP_1;
   uart_test_cfg.flow_ctrl = QL_FC_NONE;
   ql_uart_set_dcbconfig(QL_UART_PORT_1,&uart_test_cfg);
   ql_uart_open(QL_UART_PORT_1);
}

static unsigned short crc16_ccitt(const unsigned char *data, size_t length) {  
    int crc =0;
    int i,j;
    for(i=0;i<length;i++)
    {
        crc=crc^(data[i]<<8);
        for(j=0;j<8;j++)
        {
            if((crc &((int)0x8000))!=0)
            {
                crc = ((crc<<1) ^ 0x1021);
            }
            else
            {
                crc = crc << 1;
            }
        }
    }
    return (crc&0xFFFF); 
}

int y_modem_parse_filename(unsigned char *buf)
{
   int filename_offset=0;
   unsigned char name_buf[128]={0};
   unsigned char length_buf[128]={0};
   int catch_name_offset=0;
   unsigned int file_length=0;
   while(filename_offset<PACKET_SOH_SEZE-3)
   {

      if(buf[filename_offset]==0x00&&catch_name_offset==0)
      {
         memcpy(name_buf,buf,filename_offset);
         ql_uart_log("**********filename:%s ************\r\n",name_buf);
         catch_name_offset=filename_offset;
      }
      else if(catch_name_offset!=0)
      {
         if(buf[filename_offset]==0x00)
         {
            memcpy(length_buf,buf+catch_name_offset+1,filename_offset-catch_name_offset);
            ql_uart_log("**********file legth:%s bytes************\r\n",length_buf);
            file_length=atoi(length_buf) ;
            total_progress=file_length/1024;
            if((file_length%1024)!=0)
            {
               total_progress++;
            }
            return 0;
         }
      }
      filename_offset++;
   }
   return -1;

}

int write_data(unsigned char *buf,unsigned char  type)
{
   int ret=0;
   unsigned short p_crc;
   unsigned short r_crc;
   int data_size=0;
   static unsigned int offset=0;
   if(type==SOH)
   {
      data_size=PACKET_SOH_DATA_SIZE;
   }
   else 
   {
      data_size=PACKET_STX_DATA_SIZE;
   }
   p_crc=crc16_ccitt(buf,data_size);
   
   r_crc=*(buf+data_size)<<8 | *(buf+data_size+1);
   
   if(r_crc!=p_crc)
   {
      ql_uart_log("r_crc:%x p_crc:%x \r\n",r_crc,p_crc);
      return -1;
   }
   if(r_crc==0&&state==TRANS_END_EOT2)
   {
      SEND_ACK();
      ql_flash_set_security(QL_FLASH_UNPROTECT_LAST_BLOCK);
      ql_uart_log("end transfer!!!!\r\n",__FUNCTION__,__LINE__);
      bk_reboot();
      return 0;
   }
 
   ql_flash_parttion_write(QL_PARTITION_OTA, offset, buf , data_size);
   
   offset+=data_size;

   return 0;
}

static void flush_uart_fifo(void)
{
   unsigned char recv_buf[1100];
   int length=bk_uart_get_length_in_buffer(QL_UART_PORT_1);
   if(length!=0)
   {
      length=length>1100?1100:length;
      ql_uart_read(QL_UART_PORT_1, recv_buf,length);

   }

}


void ql_uart_upgrade_main()
{
   unsigned char ch;
   unsigned char package_type=0;
   unsigned char seq_num=0;
   unsigned int data_size=0;
   unsigned char recv_buf[1100];
   int get_filename=0;
   int read_package_state=RECV_HEAD;
	ql_flash_set_security(QL_FLASH_PROTECT_NONE);
   ql_flash_parttion_erase(QL_PARTITION_OTA,0,OTA_PARTITION_SIZE);
   ql_uart_log("***********ota demo test start \n");
   uart_init();
   flush_uart_fifo();
   while(1)
   {
      if(state==RECV_FILENAME&&read_package_state==RECV_HEAD)
      {
         if(get_filename!=1)
         {
            SEND_C();
            ql_uart_log("waitting host send file\r\n");
            ql_rtos_task_sleep_ms(2000);
         }
      }
      int nread_size=bk_uart_get_length_in_buffer(QL_UART_PORT_1);
      if(nread_size==0)
      {
         continue;
      }
      if(read_package_state!=RECV_DATA)
      {
         ql_uart_read(QL_UART_PORT_1, &ch,1);
      }

      if(read_package_state==RECV_HEAD)
      {
         if(ch==SOH||ch==STX||ch==EOT)
         {
            if(ch==EOT)
            {
               if(state!=TRANS_END_EOT2)
               {
                  state=TRANS_END_EOT2;
                  SEND_NACK();
                  ql_rtos_task_sleep_ms(100);
                  ql_uart_log("receive EOT 1 !!!!\r\n");
               }
               else
               {
                  SEND_ACK();
                  ql_rtos_task_sleep_ms(100);
                  SEND_C();
                  sequence_num=0;
                  ql_uart_log("receive EOT 2 !!!!\r\n");
               }
            }
            else
            {
               package_type=ch;
               read_package_state=RECV_NUMBER;
            }

         }
         else
         {
            ql_uart_log("package type error!!!\r\n");
            continue;
         }

      }
      else if(read_package_state==RECV_NUMBER)
      {
         read_package_state=RECV_NON_NUMBER;
         seq_num=ch;
         ql_uart_log("sequence number:%u\r\n",seq_num);
      }
      else if(read_package_state==RECV_NON_NUMBER)
      {
         if((char)(~ch)==(char)seq_num)
         {
            read_package_state=RECV_DATA;
            if(package_type==STX)
            {
               data_size=PACKET_STX_SEZE-3;
            }
            else
            {
               data_size=PACKET_SOH_SEZE-3;
            }
         }
         else
         {
            ql_uart_log("receive inverse number error number 1:%x number 2:%x\r\n",seq_num,ch);
            read_package_state=RECV_HEAD;
            flush_uart_fifo();
            SEND_NACK();
            continue;
         }
      }
      else if(read_package_state==RECV_DATA)
      {
         static int get_times=0;
         int length=bk_uart_get_length_in_buffer(QL_UART_PORT_1);
         if(length<data_size)
         {
            ql_rtos_task_sleep_ms(5);
            if(get_times++>RECEIVE_MAX_TIMES)
            {
               get_times=0;
               read_package_state=RECV_HEAD;
               flush_uart_fifo();
               if(state==RECV_FILENAME)
               {
                  SEND_C();
                  ql_rtos_task_sleep_ms(2000);
               }
               else
               {
                  SEND_NACK();
               }

            }
            continue;
         }
         else
         {
            get_times=0;
            length=data_size;
            ql_uart_read(QL_UART_PORT_1, recv_buf,length);
            if(state==RECV_FILENAME)
            {
               if(package_type!=SOH)
               {
                  ql_uart_log(" receive filename packet error\r\n");
                  flush_uart_fifo();
                  SEND_NACK();
                  continue;
               }
               y_modem_parse_filename(recv_buf);
               flush_uart_fifo();
               get_filename=1;
               state=TRANS_DATA;
               SEND_ACK();
               ql_rtos_task_sleep_ms(10);
               SEND_C();
               read_package_state=RECV_HEAD;
            }
            else if(state==TRANS_DATA||state==TRANS_END_EOT2)
            {
               read_package_state=RECV_HEAD;
               if(seq_num==sequence_num)
               {
                  if(write_data(recv_buf,package_type)==0)
                  {
                     SEND_ACK();
                     sequence_num++;
                     receiving_progress++;
                     ql_uart_log("progress receiving:total %u:%u\r\n",receiving_progress,total_progress);
                  }
                  else
                  {
                     ql_uart_log("crc error\r\n");
                     flush_uart_fifo();
                     SEND_NACK();
                  }

               }
               else
               {
                  ql_uart_log("error sequence \r\n");
                  flush_uart_fifo();
                  SEND_NACK();
               }
            }
         }
      }

   }

}

ql_task_t	uart_upgrade_test_thread_handle = NULL;
void ql_uart_upgrade_demo_thread_creat(void)
{
    int ret;
	ret = ql_rtos_task_create(&uart_upgrade_test_thread_handle,
		               (unsigned short)1024*6,
					   THD_EXTENDED_APP_PRIORITY,
					   "uart_upgrade_test",
					   ql_uart_upgrade_main,
					   0);


	  if (ret != kNoErr) {
		ql_uart_log("Error: Failed to create uart upgrade test thread: %d\r\n",ret);
		goto init_err;
	  }

	  return;

	  init_err:
	   if( uart_upgrade_test_thread_handle != NULL ) {
		   ql_rtos_task_delete(uart_upgrade_test_thread_handle);
	   }

}


#endif


