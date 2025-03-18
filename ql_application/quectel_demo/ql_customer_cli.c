#include "wlan_cli_pub.h"
#include "uart_pub.h"


/* Allows customers to add custom cli commands for production testing                       
*/
                                                                         				 
#define CUS_VERSION "FC41DAAR01A01.bin" //Replace with the actual version
void customer_version(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
	os_printf("get customer version \n");
	os_printf(CUS_VERSION);
}


const struct cli_command customer_clis[] =
{
	{"get_version", "get version", customer_version},
													//add command
};


int customer_cli_register(void)
{
	return cli_register_commands(customer_clis, sizeof(customer_clis) / sizeof(struct cli_command));
}

