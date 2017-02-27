/*
 * Copyright (c) 2010-2017, Arto Merilainen (arto.merilainen@gmail.com)
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

#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#define UART_BAUD_CALC(UART_BAUD_RATE,F_CPU) ((F_CPU) / ((UART_BAUD_RATE) * 16l) - 1)

volatile static char rxBuf[UART_BUFSIZE];
volatile static unsigned int rxBuf_readPtr;
volatile static unsigned int rxBuf_writePtr;
volatile static unsigned int rxTimeout;

/*
 * uart_putchar()
 *
 * Transmit a single character using the serial port. The function waits until
 * the character is transmitted
 */

static int uart_putchar(char c, FILE *stream)
{

	if (c == '\n')
		uart_putchar('\r', stream);

	/* TODO! Implement an interrupt handler for data sending... now we just
	 * wait and consume power... */

	loop_until_bit_is_set(UCSR0A, UDRE0);
	UDR0 = c;
	return 0;
}

/*
 * Interrupt routine for receiving data from the serial port. The received
 * data is placed into a ring buffer if the buffer is not full. If the buffer
 * is full, nothing is modified.
 */

SIGNAL (USART0_RX_vect)
{
	unsigned int ptr = (rxBuf_writePtr + 1) % UART_BUFSIZE;
	char c = UDR0;

	// Just a small conversion...
	if(c == '\n')
		return;
	if(c == '\r')
		c = '\n';

	// If there is room in the buffer, save the received character
	if(ptr != rxBuf_readPtr) {
		rxBuf[rxBuf_writePtr] = c;
		rxBuf_writePtr = ptr;
	}

}

/*
 * uart_count()
 *
 * Return number of bytes available in the buffer
 */

int uart_count(void)
{
	int count = rxBuf_writePtr - rxBuf_readPtr;
	if(count < 0)
		count += UART_BUFSIZE;

	return count;
}


/*
 * uart_getchar()
 *
 * Read a single character from the serial port buffer. If the buffer is
 * empty, the function blocks until a character is available.
 */

static int uart_getchar(FILE *stream)
{
	unsigned int ptr = rxBuf_readPtr;
	char cSREG = SREG;
	char cSMCR = SMCR;

	/* Make sure that the interrupts are enabled (otherwise we could be stuck
	 * in here forever..) */
	asm("sei");
	
	// Enable sleep mode
	SMCR = 0b00000001;

	// Wait for data...
	while(!uart_count()) {
		asm("sleep");
	}

	// Disable sleep mode
	SMCR =  cSMCR;

	/* Return flags-register (if the interrupts were disabled, they shall stay
	 * disabled) */
	SREG = cSREG;

	// Update pointer and return a character
	rxBuf_readPtr = (rxBuf_readPtr + 1) % UART_BUFSIZE;

	return rxBuf[ptr];
}

// Serial port stream
FILE uart_stdio = FDEV_SETUP_STREAM(uart_putchar, uart_getchar,
                                         _FDEV_SETUP_RW);

/*
 * uart_getchar()
 *
 * Read a single character from the serial port buffer. If the buffer is
 * empty, the function blocks until a character is available.
 */

void uart_init(void)
{
	rxBuf_readPtr = rxBuf_writePtr = 0;

	// Set speed
	UBRR0H = (uint8_t)(UART_BAUD_CALC(UART_BAUD,F_CPU) >> 8);
	UBRR0L = (uint8_t)UART_BAUD_CALC(UART_BAUD,F_CPU);

	// Enable transmitter, receiver and interrupt
	UCSR0B = _BV(TXEN0) | _BV(RXEN0) | _BV(RXCIE0);
	UCSR0C = _BV(UCSZ00) | _BV(UCSZ01);

	rxTimeout = 0;
	stdout = stdin = &uart_stdio;
	clearerr(stdin);
}

/*
 * uart_setTimeout()
 *
 * Set receiving timeout
 */

void uart_setTimeout(unsigned int t)
{
	rxTimeout = t;
}
