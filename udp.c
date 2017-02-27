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

#include <stdlib.h>
#include <string.h>

#include "ip.h"
#include "udp.h"
#include "config.h"

/*
 * Reference to local IP address (needed while generating a
 * UDP packet)
 */
extern char localIP[4];

/* Buffer to preserve a packet before char it... */
static char buf[256];

/* UDP-sockets */
udpSocket sockets[MAX_UDP_SOCKETS];

/*
 * udp_initialise()
 *
 * Initialise UDP-sockets
 */

void udp_initialise(void)
{
	int i;
	for(i = 0; i < MAX_UDP_SOCKETS; i++)
		sockets[i].state = SOCKETSTATE_UNUSED;
}

/*
 * udp_send(dest, lport, dPort, msg, len)
 *
 * Send an UDP-packet
 */

void udp_send(char *dest, unsigned int lPort, unsigned int dPort,
			char *msg, unsigned int len)
{
	unsigned int checksum;
	udpPseudoHeader * newPacketPH = (udpPseudoHeader *)&buf;
	udpPacket * newPacket = (udpPacket *)&buf[12];

	/* Generate a pseudo header */
	memcpy(newPacketPH->sourceIP, localIP, 4);
	memcpy(newPacketPH->destIP, dest, 4);
	newPacketPH->protocol=IPPACKETTYPE_UDP;
	newPacketPH->zeroByte=0x00;
	newPacketPH->pLen[0] = (len + 8) >> 8;
	newPacketPH->pLen[1] = (len + 8) & 0xFF;

	/* Generate the real header */
	newPacket->lPort[0] = lPort >> 8;
	newPacket->lPort[1] = lPort & 0xFF;
	newPacket->dPort[0] = dPort >> 8;
	newPacket->dPort[1] = dPort & 0xFF;
	newPacket->len[0] = (len + 8) >> 8;
	newPacket->len[1] = (len + 8) & 0xFF;
	newPacket->checksum[0] = 0x00;
	newPacket->checksum[1] = 0x00;

	/* Copy message */
	memcpy(buf + 20, msg, len);

	/* Calculate a checksum for the packet */
	checksum = ip_calculateChecksum(buf, len + 20);

	/* Insert checksum */
	newPacket->checksum[0] = checksum >> 8;
	newPacket->checksum[1] = checksum & 0xFF;

	/* Send */
	ip_send(dest, IPPACKETTYPE_UDP, &buf[12], len + 8);
}


/*
 * udp_register(port, dbuf, dMaxLen)
 *
 * Register an UDP-socket for listening.
 */
udpSocket * udp_register(unsigned int port, char * dbuf,
							unsigned int dMaxLen)
{
	unsigned int i;
	for(i = 0; (i < MAX_UDP_SOCKETS) &&
		(sockets[i].state != SOCKETSTATE_UNUSED); i++) ;

	if(i < MAX_UDP_SOCKETS)
	{
		sockets[i].state = SOCKETSTATE_WAITING;
		sockets[i].localPort = port;
		sockets[i].dbuf = dbuf;
		sockets[i].dMaxLen = dMaxLen;
		return &sockets[i];
	}
	return 0;
}

/*
 * udp_reregister(socket)
 *
 * Reregister an UDP-socket after receiving a message
 */
void udp_reregister(udpSocket * socket)
{
	if(socket->state == SOCKETSTATE_ESTABLISHED)
		socket->state = SOCKETSTATE_WAITING;
}

/*
 * udp_disconnect(socket)
 *
 * Release an udp socket
 */
void udp_disconnect(udpSocket *socket)
{
	socket->state = SOCKETSTATE_UNUSED;
}

/*
 * udp_handle()
 *
 * Receive an UDP packet. The routine checks if received UDP-packet
 * is valid and if we're listening the port.
 */
void udp_handle(void *packetData)
{
	unsigned int i;

	ipHeader *header = packetData;
	udpPacket * packet = (void *)(((unsigned int)packetData) +
		((header->verHLen & 0x0F) * 4));

	/* Search socket */
	for(i = 0; i < MAX_UDP_SOCKETS; i++) {

		/* First, find the socket */
		if((sockets[i].state == SOCKETSTATE_WAITING) &&
			(sockets[i].localPort ==
			((packet->dPort[0] << 8) | packet->dPort[1]))) {

			/* Is there room enough? */
			if((((packet->len[0] << 8) | packet->len[1]) - 8) <
				sockets[i].dMaxLen) {

				/* Yes */
				sockets[i].state = SOCKETSTATE_ESTABLISHED;
				sockets[i].dLen =
					(((packet->len[0] << 8) | packet->len[1]) - 8);
				memcpy(sockets[i].dbuf, (char *)packet + 8,
					(((packet->len[0] << 8) | packet->len[1]) - 8));
				memcpy((char *)sockets[i].sourceIP, header->sourceIP, 4);
			}
		}
	}
}
