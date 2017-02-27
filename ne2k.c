/*
 * Copyright (c) 2007-2017, Arto Merilainen (arto.merilainen@gmail.com)
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

#include <ctype.h>
#include <stdio.h>
#include <stdint.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "ne2k.h"
#include "ip.h"
#include "uart.h"
#include "config.h"

/*
 * TODO! MAC address is defined here although it should be read from the
 * eeprom on the network adapter
 */

char localMAC[6] = {'N', 'E', 'X', 'T', 'O', 'N'};

/*
 * Few hard coded local variables...
 */

static char packetHeader[4];
static char packetData[NE2K_RX_BUF_SIZE];

/*
 * Communication with the NIC is done using the following routines
 */

static void NIC_WRITE(unsigned int addr, char data)
{	
	NIC_ADDR(addr);
	_delay_us(1);
	DDRA = 0xFF;
	NIC_DATA_OUT = data;
	_delay_us(1);    
	NIC_CNTRL_PORT &= ~(_BV(NIC_IOWB));	
	_delay_us(1);
	NIC_CNTRL_PORT |= _BV(NIC_IOWB);
	DDRA = 0x00;
}


static char NIC_READ(unsigned int addr)
{
	char data;
	
	DDRA = 0x00;
	
	NIC_ADDR(addr);
	_delay_us(1);
	NIC_CNTRL_PORT &= ~(_BV(NIC_IORB));
	_delay_us(1);
	data = NIC_DATA_IN;
	NIC_CNTRL_PORT |= _BV(NIC_IORB);
	
	return data;
}

/*
 * Initialize ne2k network interface device
 */

void ne2k_init(void)
{

	char tmp0;
	int cnt;

	EIMSK = 0x01;
	EICRA = 0b000000011;

	NIC_RESET_PORT |= _BV(NIC_RESET);
	_delay_ms(1);
	NIC_RESET_PORT &= ~(_BV(NIC_RESET));

	// Reset the NIC
	tmp0 = NIC_READ(PORT_RESET);
	NIC_WRITE(PORT_RESET, tmp0);
	_delay_ms(10);
	NIC_WRITE(PORT_CMD, CMD_STP | CMD_RD2);

	// Reset DMA registers
	NIC_WRITE(PORT_RBCR0, 0x00);
	NIC_WRITE(PORT_RBCR1, 0x00);

	/* Set the NIC into loopback mode, so it can't receive any packets while
	 * initialising... 
	 */
	NIC_WRITE(PORT_RCR, RCR_MON | RCR_DEF);

	/* Data configuration register (disable word transfer, internal loopback and
	 * enable fifo threshold bit 1)
	 */
	NIC_WRITE(PORT_DCR, DCR_LS | DCR_FT1 | DCR_ARM | DCR_DEF);

	/* Transmit configuration register (enable nothing particular) */
	NIC_WRITE(PORT_TCR, TCR_DEF);

	/* This is quite an ugly part... we're assuming that there is 16kB _and_ it
	 * is mapped to start at ioAddress 0x4000
	 */
	NIC_WRITE(PORT_PSTART, 0x46);
	NIC_WRITE(PORT_PSTOP, 0x60);


	/* Set boundary and curr registers */
	NIC_WRITE(PORT_BNRY, 0x46);
	NIC_WRITE(PORT_CMD, CMD_PAGE1 | CMD_RD2 | CMD_STP);
	NIC_WRITE(PORT_CURR, 0x47);

	/* Load MAC address */
	for(cnt = 0; cnt < 6; cnt++)
		NIC_WRITE(PORT_PAR0 + cnt, localMAC[cnt]);

	/* Set multicast address registers */
	for(cnt = 0; cnt < 6; cnt++)
		NIC_WRITE(PORT_MAR0 + cnt, 0xFF);

	NIC_WRITE(PORT_CMD, CMD_RD2 | CMD_STA);

	/* Receive configuration register (accept broadcast packets) */
	NIC_WRITE(PORT_RCR, RCR_AB | RCR_DEF);

	/* Clear interrupt status register */
	NIC_WRITE(PORT_ISR, 0xFF);

	/* Enable received interrupt */
	NIC_WRITE(PORT_IMR, 0x01);
}

/*
 * Receive interrupt handler
 */
SIGNAL (SIG_INTERRUPT0) 
{

	unsigned int cnt, packetSize;
	char status;
	char currPage, lastPage;
	
	NIC_WRITE(PORT_CMD, CMD_RD2);
	NIC_WRITE(PORT_ISR, 0xFF);
	NIC_WRITE(PORT_CMD, CMD_RD2 | CMD_STA);

	/* Did the NIC receive the packet properly? */
	status = NIC_READ(PORT_RSR);

	if(status & 0x01) {

		/* Disable receiver */
		NIC_WRITE(PORT_RCR, RCR_MON);

		/* Read the value of CURR-register */
		NIC_WRITE(PORT_CMD, CMD_RD2 | CMD_PAGE1);
		lastPage = NIC_READ(PORT_CURR);
		NIC_WRITE(PORT_CMD, CMD_RD2);

		currPage = 0x47;

		while(lastPage != currPage) {
			NIC_WRITE(PORT_RSAR0, 0x00);
			NIC_WRITE(PORT_RSAR1, currPage);
			NIC_WRITE(PORT_RBCR0, 0x04);
			NIC_WRITE(PORT_RBCR1, 0x00);

			NIC_WRITE(PORT_CMD, CMD_RD0);

			for(cnt=0; cnt < 4; cnt++)
				packetHeader[cnt] = NIC_READ(PORT_DMA);

			packetSize = (packetHeader[3] << 8) | packetHeader[2];

			if((packetSize > 4) && (packetSize < sizeof(packetData))) {

				packetSize-=4;

				NIC_WRITE(PORT_RSAR0, 0x04);
				NIC_WRITE(PORT_RSAR1, currPage);
				NIC_WRITE(PORT_RBCR0, packetSize & 0xFF);
				NIC_WRITE(PORT_RBCR1, packetSize >> 8);
				NIC_WRITE(PORT_CMD, CMD_RD0);

				for(cnt=0;cnt<packetSize; cnt++)
					packetData[cnt]=NIC_READ(PORT_DMA);

				packet_receive((etherPacket *) packetData);
			}

			currPage = packetHeader[1];
		}

		/* Reset boundary and current page registers */
		NIC_WRITE(PORT_BNRY, 0x46);
		NIC_WRITE(PORT_CMD, CMD_RD2 | CMD_PAGE1);
		NIC_WRITE(PORT_CURR, 0x47);
		NIC_WRITE(PORT_CMD, CMD_RD2);

		/* Enable receiver */
		NIC_WRITE(PORT_RCR, RCR_AB);
	} else {
		if(status & 0x06)
			NIC_WRITE(PORT_CURR, 0x46);	
	}
}


void ne2k_send(char *net_addr, char *msg, unsigned int length,
			   unsigned int type, unsigned int intstatus)
{
	unsigned int cnt;
	unsigned int packetLength;

	/* Disable interrupts to avoid concurrency issues with the receiver */
	cli();
	_delay_us(1);

	/* Calculate the actual packet length */
	if(length>=46)
		packetLength = length + 14;
	else
		packetLength = 60;

	/* Select first page */
	NIC_WRITE(PORT_CMD, CMD_RD2 | CMD_STA);

	/* Inform that we're going to write data using DMA */
	NIC_WRITE(PORT_RSAR1, 0x40);
	NIC_WRITE(PORT_RSAR0, 0x00);
	NIC_WRITE(PORT_RBCR1, (char)(packetLength >> 8));
	NIC_WRITE(PORT_RBCR0, (char)(packetLength & 0xFF));
	NIC_WRITE(PORT_CMD, CMD_RD1 | CMD_STA);

	/* Send the destination address */
	for(cnt=0;cnt<6;cnt++)
		NIC_WRITE(PORT_DMA, net_addr[cnt]);

	/* Send the source address */
	for(cnt=0;cnt<6;cnt++)
		NIC_WRITE(PORT_DMA, localMAC[cnt]);

	/* Send the packet type */
	NIC_WRITE(PORT_DMA, (char)(type >> 8));
	NIC_WRITE(PORT_DMA, (char)(type & 0xFF));

	/* ..and finally, send the data */
	for(cnt=0;cnt<length;cnt++)
		NIC_WRITE(PORT_DMA, msg[cnt]);

	/* Fill rest of the packet if the data part is less than 46 bytes long */
	if (length < 46) {
		for(; cnt < 46; cnt++)
			NIC_WRITE(PORT_DMA, 0x00);
	}

	/* Stop the DMA operation (if it's not already finished) */
	NIC_WRITE(PORT_CMD, CMD_RD2 | CMD_STA);

	NIC_WRITE(PORT_TPSR, 0x40);
	NIC_WRITE(PORT_TBCR1, (char)(packetLength >> 8));
	NIC_WRITE(PORT_TBCR0, (char)(packetLength & 0xFF));

	/* Send the packet */
	NIC_WRITE(PORT_CMD, CMD_RD1 | CMD_RD2 | CMD_TXP | CMD_STA);

	/* Re-enable interrupts */
	sei();
}
