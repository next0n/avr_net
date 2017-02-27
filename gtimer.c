/*
 * Copyright (c) 2006-2017, Arto Merilainen (arto.merilainen@gmail.com)
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
#include "gtimer.h"
#include "tcp.h"
#include "ip.h"

#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>

volatile unsigned int globalTimer;

int blinkFQ;

extern addrCL arpTable[MAX_ARP_ENTRIES];

/*
 * Timer 0 signal handler. This routine:
 * - Keeps the ARP table up-to-date,
 * - Updates globalTimer variable to allow timeouts
 * - Blinks the "alive" LED
 */
SIGNAL (TIMER0_OVF_vect)
{
	globalTimer++;
	tcp_sustain();
	
	int i;
	for(i = 0; i < MAX_ARP_ENTRIES; i++) {
		if(arpTable[i].state == ARPSTATE_ENABLED) {
			arpTable[i].lifeTime--;
			if(arpTable[i].lifeTime == 0)
				arpTable[i].state = ARPSTATE_DISABLED;
		}
	}

	if(!(PORTD & _BV(6)))
		PORTD |= _BV(6);
	if(globalTimer % blinkFQ == 0)
		PORTD &= ~(_BV(6));
}

/*
 * gtimer_init()
 *
 * Initialize timer 0. The timer is used to sustain globalTimer variable,
 * which is used for implementing timeouts
 */
void gtimer_init(void)
{
	TCCR0A = 0x00;
	TCCR0B = 0b00000100;
	TIMSK0 = 0b00000001;
	OCR0A = OCR0B = 0xFF;

	blinkFQ = 100;
}
