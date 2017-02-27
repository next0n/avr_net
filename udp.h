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

#ifndef UDP_H
#define UDP_H

typedef struct {
	/* Pseudo header */
	char sourceIP[4];
	char destIP[4];
	char zeroByte;
	char protocol;
	char pLen[2];
} udpPseudoHeader;

typedef struct {
	/* UDP header */
	char lPort[2];
	char dPort[2];
	char len[2];
	char checksum[2];
} udpPacket;

typedef struct {
	volatile unsigned int state;
	unsigned int localPort;
	volatile char sourceIP[4];
	char *dbuf;
	volatile unsigned int dLen;
	unsigned int dMaxLen;
} udpSocket;

void udp_initialise(void);
void udp_handle(void *packetData);
udpSocket * udp_register(unsigned int port, char * dbuf, unsigned int dMaxLen);
void udp_reregister(udpSocket * socket);
void udp_disconnect(udpSocket *socket);
void udp_send(char *dest, unsigned int lPort, unsigned int dPort, char *msg, unsigned int len);

#define SOCKETSTATE_UNUSED        0
#define SOCKETSTATE_WAITING       1
#define SOCKETSTATE_ESTABLISHED   2

#endif
