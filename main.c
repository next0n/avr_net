/*
 * Copyright (c) 2011-2017, Arto Merilainen (arto.merilainen@gmail.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "uart.h"
#include "ne2k.h"
#include "ip.h"
#include "tcp.h"
#include "udp.h"
#include "gtimer.h"
#include "fifo.h"
#include "fileops.h"
#include "httpd.h"

#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <avr/io.h>
#include <util/delay.h>
#include <string.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <avr/io.h>


/*
 * Default IP settings are stored here
 */

const char DEFAULTIP[4]	=	{192, 168, 2,  156};
const char DEFAULTMASK[4]	=	{255, 255, 255, 0};
const char DEFAULTGATEWAY[4] =	{192, 168, 2, 1};

struct {
	char flag;
	char ip[4];
	char mask[4];
	char gateway[4];
} eepromConfiguration;

/*
 * init()
 *
 * Board initialization
 */

void board_init(void)
{

	// Initialize ports

	/* Setup port B (nothing special) */
	DDRB = 0b11111111;
	/* Setup port D (notice INT0 and serial line) */
	DDRD = 0b01111010;
	/* Setup port C (NE2K control port) */
	DDRC = 0b11111111;
	/* Setup port A (NE2K data port) */
	DDRA = 0b11111111;

	/* Enable addres 0, deactivate RD and WR signals */
	PORTC = 0b01100000;

	/* Setup some sensible values.. */
	PORTD = 0b11111001;
	PORTB = 0xFF;
	PORTA = 0x00;

	/* Initialize serial port and global timer */
	gtimer_init();
	uart_init();

	/* Tell that the device is alive */
	printf("AVR based network server\n");

	/* Setup NIC */
	ne2k_init();

	/* Enable interrupts */
	asm("sei");

	/* Read configuration from EEPROM */
	eeprom_read_block(&eepromConfiguration, 0, sizeof(eepromConfiguration));

	/* Check for magic in EEPROM data */
	if(eepromConfiguration.flag != 0xAB) {
		/* If there's a mismatch, reinitialize EEPROM */
		eepromConfiguration.flag = 0xAB;
		memcpy(eepromConfiguration.ip, DEFAULTIP, 4);
		memcpy(eepromConfiguration.gateway, DEFAULTGATEWAY, 4);
		memcpy(eepromConfiguration.mask, DEFAULTMASK, 4);
		eeprom_write_block(&eepromConfiguration, 0, sizeof(eepromConfiguration));
	}

	/* Initialize ip stack */
	ip_initialise(eepromConfiguration.ip,
		eepromConfiguration.gateway,
		eepromConfiguration.mask);
	udp_initialise();
	tcp_initialise();
}

/*
 * "Flash File System"
 * Include all files as variables and create an array of the files
 */

include_data(indexPage, indexPage_end,
			 "C:/Users/Arto Merilainen/Development/avr_net/index.html");
include_data(stylePage, stylePage_end,
			 "C:/Users/Arto Merilainen/Development/avr_net/style.css");
include_data(start_phtml, start_phtml_end,
			 "C:/Users/Arto Merilainen/Development/avr_net/page_start.phtml");
include_data(end_phtml, end_phtml_end,
			 "C:/Users/Arto Merilainen/Development/avr_net/page_end.phtml");
include_data(pic, pic_end,
			 "C:/Users/Arto Merilainen/Development/avr_net/avr_server.jpg");

char * myFiles[] = {
"/index.html",			indexPage,		indexPage_end,
"/style.css",			stylePage,		stylePage_end,
"/page_end.phtml",		end_phtml,		end_phtml_end,
"/page_start.phtml",	start_phtml,	start_phtml_end,
"/avr_server.jpg",		pic,			pic_end,
NULL,					NULL,			NULL
};

/*
 * HTTP callback
 */
char buf[32];
int callback(char * requestType, char *uri)
{
	httpd_get_uri_filename(uri, buf, sizeof(buf));

	if(!strcmp(buf, "/led.html")) {

		if(strcmp(requestType, "GET"))
			return -1;

		httpd_transmit_ok_header();
		printf("Cache-Control: no-cache\n"
			"Pragma: no-cache\n"
			"Expires: -1\n\n");

		httpd_transmit_file("/page_start.phtml");
		printf("<h1>Led Control</h1>\n<hr>");

		char lbuf[5];
		int i = 0;
		for(i = 0; i < 4; i++) {

			sprintf(lbuf, "led%1u", i);
			if(httpd_get_uri_param(uri, lbuf, buf, sizeof(buf)) > 0) {
				if(!strcmp(buf, "0"))
					PORTD |= _BV(3 + i);
				if(!strcmp(buf, "1"))
					PORTD &= ~(_BV(3 + i));
			}
			printf("<p>Turn <a href=\"led.html?led%1u=1\">on</a>/"
				"<a href=\"led.html?led%u=0\">off</a> led %u</p>", i, i, i);
		}

		
		httpd_transmit_file("/page_end.phtml");
		return 0;
	}

	if(!strcmp(buf, "/ip.html")) {

		char *ptr;
		int dataCopied = 0;
		if(httpd_get_uri_param(uri, "ip", buf, sizeof(buf)) > 0) {
			ptr = eepromConfiguration.ip;
			if(sscanf(buf, "%hhu.%hhu.%hhu.%hhu", ptr, ptr + 1, ptr + 2, ptr + 3)
				== 4) {
				fprintf(&uart_stdio, "IP OK!\n");
				dataCopied++;
			}
		}

		if(httpd_get_uri_param(uri, "gateway", buf, sizeof(buf)) > 0) {
			fprintf(&uart_stdio, "Gateway is a param: %s\n\n", buf);
			ptr = eepromConfiguration.gateway;
			if(sscanf(buf, "%hhu.%hhu.%hhu.%hhu", ptr, ptr + 1, ptr + 2, ptr + 3)
				== 4) {
				fprintf(&uart_stdio, "Gateway OK!\n");
				dataCopied++;
			}
		}

		if(httpd_get_uri_param(uri, "mask", buf, sizeof(buf)) > 0) {
			ptr = eepromConfiguration.mask;
			if(sscanf(buf, "%hhu.%hhu.%hhu.%hhu", ptr, ptr + 1, ptr + 2, ptr + 3)
				== 4) {
				fprintf(&uart_stdio, "Netmask OK!\n");
				dataCopied++;
			}
		}

		httpd_transmit_ok_header();
		printf("Cache-Control: no-cache\n"
			"Pragma: no-cache\n"
			"Expires: -1\n");

		if(dataCopied == 3) {
			eeprom_write_block(&eepromConfiguration, 0, sizeof(eepromConfiguration));
			ptr = eepromConfiguration.ip;
			printf("Refresh: 3 ; url=http://%u.%u.%u.%u/ip.html\n\n",
				ptr[0], ptr[1], ptr[2], ptr[3]);

			httpd_transmit_file("/page_start.phtml");
			printf("Please wait... you will be redirected.\n");
			httpd_transmit_file("/page_end.phtml");

			return 1;

		} else if(dataCopied > 0) {
			eeprom_read_block(&eepromConfiguration, 0, sizeof(eepromConfiguration));
		}

		printf("\n");

		httpd_transmit_file("/page_start.phtml");

		/* Page title */
		printf("<h1>IP Settings</h1>\n<hr>");

		/* Form begin */
		printf("<form method=\"get\" action=\"ip.html\">\n");

		printf("<table>\n");

		/* IP */
		ptr = eepromConfiguration.ip;
		printf("<tr><td>IP Address:</td><td><input name=\"ip\""
			"value=\"%u.%u.%u.%u\"></td></tr>\n", ptr[0], ptr[1], ptr[2], ptr[3]);
		
		ptr = eepromConfiguration.mask;
		printf("<tr><td>Network Mask:</td><td><input name=\"mask\""
			"value=\"%u.%u.%u.%u\"></td></tr>\n", ptr[0], ptr[1], ptr[2], ptr[3]);

		ptr = eepromConfiguration.gateway;
		printf("<tr><td>Gateway Address:</td><td><input name=\"gateway\" "
			"value=\"%u.%u.%u.%u\"></td></tr>\n" , ptr[0], ptr[1], ptr[2], ptr[3]);
		printf("</table>\n");

		/* Form end */
		printf("<p><input type=\"submit\" value=\"Save and Reboot\"></p>\n"
			"</form>\n");
		
		httpd_transmit_file("/page_end.phtml");
		return 0;
	}

	return -1;

}

/*
 * main()
 *
 * Main program
 */

int main(void)
{
	board_init();

	httpd_start(80, myFiles, callback);

	return 0;
}
