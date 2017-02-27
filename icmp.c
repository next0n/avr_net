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
#include "icmp.h"
#include "config.h"

/*
 * Reference to local IP address
 */

extern char localIP[4];

/*
 * icmp_handle(packetData)
 *
 * Handle an ICMP-packet. At the moment the only supported packet type is
 * "echo request" (aka ping).
 */

void icmp_handle(void *packetData)
{
	ipHeader * header = packetData;
	icmpPacket * receivedPacket = (void *)(((unsigned int)packetData) +
		((header->verHLen & 0x0F) * 4));
	unsigned int checksum, packetLen;
	char *ptr;

	packetLen = (unsigned int)(header->tLen[0] << 8 | header->tLen[1]) -
		((header->verHLen & 0x0F) * 4);

	switch(receivedPacket->type)
	{
		case ICMPPACKET_ECHO_REQUEST:

			/* NOTE! We are using the original packet structure for
			 * answering, which is usually a very ugly thing to do... */

			receivedPacket->type = ICMPPACKET_ECHO_REPLY;

			receivedPacket->checksum[0]=0x00;
			receivedPacket->checksum[1]=0x00;

			ptr = (void *)(((unsigned int)packetData) + ((header->verHLen & 0x0F) * 4));
			checksum = ip_calculateChecksum(ptr, packetLen);

			receivedPacket->checksum[0] = checksum >> 8;
			receivedPacket->checksum[1] = checksum & 0xFF;
 
			memcpy(header->destIP, header->sourceIP, 4);
			memcpy(header->sourceIP, localIP, 4);

			/* Send the packet */

			ip_send(header->destIP, IPPACKETTYPE_ICMP, receivedPacket, packetLen);

			break;
	}
}
