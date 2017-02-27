/*
 * Copyright (c) 2008-2017, Arto Merilainen (arto.merilainen@gmail.com)
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


#ifndef TCP_H

#define TCP_H

#include "fifo.h"

#include <stdio.h>

typedef struct {
	/* Pseudo header */
	unsigned char sourceIP[4];
	unsigned char destIP[4];
	unsigned char zeroByte;
	unsigned char protocol;
	unsigned char pLen[2];
} tcpPseudoHeader;

typedef struct {
	/* TCP header */
	unsigned char lPort[2];
	unsigned char dPort[2];
	unsigned char seqNum[4];
	unsigned char ackNum[4];
	unsigned char headerSize;
	unsigned char codeBits;
	unsigned char receiveWindow[2];
	unsigned char checksum[2];
	unsigned char urgent[2];
} tcpPacket;

typedef struct {
	volatile unsigned int state;
	unsigned int localPort;
	unsigned int remotePort;
	char destIP[4];
		
	/* These variables are used to trace packet losts */
	unsigned char ackNum[4];
	unsigned char seqNum[4];
	unsigned int ackState;
	unsigned char retryCounter;
		
	/* Datastream */
	stream strm;

	FILE stdio;
	unsigned int streamTimeout;
	unsigned int lastWindowSize;

	fifo fsBuf;

} tcpSocket;


void tcp_initialise(void);
tcpSocket * tcp_reserveSocket(void * inBuf, void * outBuf, void * fsBuf,
								unsigned int inBufSize,
								unsigned int outBufSize);

void tcp_listen(tcpSocket * socket);
void tcp_connect(tcpSocket * socket);
void tcp_disconnect(tcpSocket *socket);
void tcp_handle(void *packetData);
void tcp_sustain(void);
void tcp_setTimeout(tcpSocket * socket, unsigned int t);
void tcp_flush(tcpSocket *socket);

#define TCP_TOTAL_RETRIES			2
#define TCP_RETRY_INTERVAL			1000

#define TCPSOCKETSTATE_UNUSED		0
#define TCPSOCKETSTATE_LISTEN		1
#define TCPSOCKETSTATE_SYN_SENT		2
#define TCPSOCKETSTATE_SYN_RECEIVED	3
#define TCPSOCKETSTATE_ESTABLISHED	4
#define TCPSOCKETSTATE_FIN_WAIT_1	5
#define TCPSOCKETSTATE_FIN_WAIT_2	6
#define TCPSOCKETSTATE_CLOSE_WAIT	7
#define TCPSOCKETSTATE_CLOSING		8
#define TCPSOCKETSTATE_LAST_ACK		9
#define TCPSOCKETSTATE_TIME_WAIT	10
#define TCPSOCKETSTATE_CLOSED		11
#define TCPSOCKETSTATE_UNKNOWN		12

#define TCPFLAGS_CWR				0x80
#define TCPFLAGS_ECE				0x40
#define TCPFLAGS_URG				0x20
#define TCPFLAGS_ACK				0x10
#define TCPFLAGS_PSH				0x08
#define TCPFLAGS_RST				0x04
#define TCPFLAGS_SYN				0x02
#define TCPFLAGS_FIN				0x01

#define TCPPACKETTYPE_NULL			0
#define TCPPACKETTYPE_EXISTS		1
#define TCPPACKETTYPE_URGENT		2

#endif
