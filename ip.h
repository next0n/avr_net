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

#ifndef IP_H
#define IP_H

typedef struct
{
	char packetReceiver[6];
	char packetSender[6];
	char packetType[2];
	char packetData[];
} etherPacket;

typedef struct
{
	char hHTYPE;
	char lHTYPE;
	char hPTYPE;
	char lPTYPE;
	char HLEN;
	char PLEN;
	char hOPER;
	char lOPER;
	char senderHWA[6];
	char senderIP[4];
	char receiverHWA[6];
	char receiverIP[4];
} arpPacket;

typedef struct
{
	char verHLen;
	char TOS;
	char tLen[2];
	char ID[2];
	char flgFrgOffset[2];
	char timeToLive;
	char protocol;
	char checksum[2];
	char sourceIP[4];
	char destIP[4];
} ipHeader;

typedef struct
{
	volatile char state;
	volatile char IP[4];
	volatile char MAC[6];
	volatile unsigned int lifeTime;
} addrCL;



void ip_initialise(const char * ip, const char * gateway, const char * nmask);
unsigned int ip_calculateChecksum(char *ptr, unsigned int len);
void ip_send(char *ip, char protocol, void *message, unsigned int msgLen);
unsigned int arp_sendquery(char *ip);
void arp_handle(etherPacket *packetData);
void packet_receive(etherPacket *packetData);
void arp_sendAliveQuery(char *ip);
void ip_initialise_dhcp(void);

#define PACKETTYPE_ARP          0x806
#define PACKETTYPE_IP           0x800

#define IPPACKETTYPE_ICMP       0x01
#define IPPACKETTYPE_UDP        17
#define IPPACKETTYPE_TCP        0x06

#define ARPSTATE_DISABLED       0x00
#define ARPSTATE_WAITING        0x01
#define ARPSTATE_ENABLED        0x02

#endif
