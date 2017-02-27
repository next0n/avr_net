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

#ifndef NE2K_H
#define NE2K_H

void ne2k_init(void);
void ne2k_send(char *net_addr, char *msg, unsigned int length, unsigned int type, unsigned int intstatus);

#define NIC_DATA_PORT		PORTA
#define NIC_CNTRL_PORT		PORTC
#define NIC_RESET_PORT		PORTC
#define NIC_IOWB			5
#define NIC_IORB			6
#define NIC_RESET			7
#define NIC_ADDR(ADDR)		PORTC = (PORTC & 0xE0) | ADDR
#define NIC_DATA_OUT		PORTA
#define NIC_DATA_IN			PINA

/* The declaration list of commands is not complete.. there are only commands the driver needs */

#define PORT_CMD    0x00

/* Page 0 (and partly page 2) registers */
#define PORT_PSTART 0x01
#define PORT_PSTOP  0x02
#define PORT_BNRY   0x03
#define PORT_TSR    0x04
#define PORT_TPSR   0x04
#define PORT_TBCR0  0x05
#define PORT_TBCR1  0x06
#define PORT_ISR    0x07
#define PORT_RSAR0  0x08
#define PORT_RSAR1  0x09
#define PORT_RBCR0  0x0A
#define PORT_RBCR1  0x0B
#define PORT_RSR    0x0C
#define PORT_RCR    0x0C
#define PORT_TCR    0x0D
#define PORT_DCR    0x0E
#define PORT_IMR    0x0F

/* Page 1 registers */
#define PORT_PAR0   0x01
#define PORT_PAR1   0x02
#define PORT_PAR2   0x03
#define PORT_PAR3   0x04
#define PORT_PAR4   0x05
#define PORT_PAR5   0x06
#define PORT_CURR   0x07
#define PORT_MAR0   0x08
#define PORT_MAR1   0x09
#define PORT_MAR2   0x0A
#define PORT_MAR3   0x0B
#define PORT_MAR4   0x0C
#define PORT_MAR5   0x0D
#define PORT_MAR6   0x0E
#define PORT_MAR7   0x0F

#define PORT_RESET  0x1F
#define PORT_DMA    0x10

#define CMD_STP     0x01
#define CMD_STA     0x02
#define CMD_TXP     0x04
#define CMD_RD0     0x08
#define CMD_RD1     0x10
#define CMD_RD2     0x20
#define CMD_PAGE1   0x40
#define CMD_PAGE2   0x80

#define ISR_PRX     0x01
#define ISR_PTX     0x02

#define DCR_DEF     0x80
#define DCR_LS      0x08
#define DCR_FT1     0x40
#define DCR_ARM     0x10

#define TCR_DEF     0xE0

#define RCR_DEF     0xC0
#define RCR_AB      0x04
#define RCR_MON     0x20

#define RSR_PRX     0x01
#define RSR_CRC     0x02

#endif
