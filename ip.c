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

#include "ip.h"
#include "icmp.h"
#include "udp.h"
#include "dhcp.h"
#include "tcp.h"
#include "gtimer.h"

#include "ne2k.h"
#include <stdio.h>

#include <string.h>

#include "config.h"

const char broadcastMAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
const char broadcastIP[4] = {255, 255, 255, 255};

/*
 * IP-address configuration
 */

char localIP[4];
char gatewayIP[4];
char netmask[4];

/*
 * External variables
 */

extern char localMAC[6];

/*
 * ARP-table
 */

addrCL arpTable[MAX_ARP_ENTRIES];

/*
 * ip_initialise(ip, gateway, nmask)
 *
 * This function initializes IP stack. The function parameters determine
 * the local IP, gateway and network mask addresses.
 */

void ip_initialise(const char * ip, const char * gateway, const char * nmask)
{
	unsigned int i;
	printf("Initialising IPv4... ");

	/* Initialise ARP-table */
	for(i = 0; i < MAX_ARP_ENTRIES; i++)
		arpTable[i].state = ARPSTATE_DISABLED;

	/* IP-address configuration */
	
	memcpy(localIP, ip, 4);
	memcpy(gatewayIP, gateway, 4);
	memcpy(netmask, nmask, 4);

	printf("Using IP address: %u.%u.%u.%u\n", localIP[0], localIP[1],
		localIP[2], localIP[3]);

}

/*
 * ip_calculateChecksum(ptr, len)
 *
 * Calculate checksum for IP-packet. This checksum calculation is used in all
 * different packet types. The basic idea is to take logical inverse of each
 * word in the packet and sum these up.
 */

unsigned int ip_calculateChecksum(char *ptr, unsigned int len)
{
	unsigned int cnt, checksum, newChecksum;

	for(checksum = 0xFFFF, cnt=0; cnt < len; cnt++) {
		newChecksum = checksum - (cnt % 2 ? ptr[cnt] : ptr[cnt] << 8);
		if(newChecksum > checksum) {
			newChecksum--;
		}
		checksum = newChecksum;
	}

	return checksum;
}

void ip_send(char *ip, char protocol, void *message, unsigned int msgLen)
{
	unsigned int arpQueryID, cnt, checksum;
	unsigned int useGW;
	char ipBuf[160];

	ipHeader header;

	if(msgLen + sizeof(ipHeader) > sizeof(ipBuf))
		return;

	memcpy(header.sourceIP, localIP, 4);
	memcpy(header.destIP, ip, 4);

	header.verHLen =  (char)(0x05 | (20 << 4));
	header.TOS = 0x00;
	header.tLen[0] = (msgLen + 20) >> 8;
	header.tLen[1] = (msgLen + 20) & 0xFF;
	header.ID[0]=0x04;
	header.ID[1]=0x00;
	header.flgFrgOffset[0]=0x40;
	header.flgFrgOffset[1]=0x00;
	header.timeToLive=0x80;
	header.protocol = protocol;
	header.checksum[0] = header.checksum[1] = 0x00;

	memcpy(header.destIP, ip, 4);
	memcpy(header.sourceIP, localIP, 4);

	/* Calculate a checksum for the packet */

	checksum = ip_calculateChecksum((char *) &header, 20);

	header.checksum[0] = checksum >> 8;
	header.checksum[1] = checksum & 0xFF;

	/* Copy the header and data into buffer */
	memcpy(ipBuf, (char *) &header, 20);
	memcpy(ipBuf + 20, (char *) message, msgLen);

	/* Skip ARP search if we're working on broadcast message */
	if(memcmp((char *)&broadcastIP, ip, 4)) {
		if((netmask[0] & header.destIP[0]) !=
			(netmask[0] & header.sourceIP[0]) || 
			(netmask[1] & header.destIP[1]) !=
			(netmask[1] & header.sourceIP[1]) || 
			(netmask[2] & header.destIP[2]) !=
			(netmask[2] & header.sourceIP[2]) || 
			(netmask[3] & header.destIP[3]) !=
		 	(netmask[3] & header.sourceIP[3])) {

				for(cnt = 0; memcmp(gatewayIP, (char *)arpTable[cnt].IP, 4) &&
					cnt < MAX_ARP_ENTRIES; cnt++) ;

				useGW = 1;
		} else {
				for(cnt=0; ((cnt < MAX_ARP_ENTRIES) &&
					((memcmp(header.destIP, (char *)arpTable[cnt].IP, 4)) ||
					(arpTable[cnt].state != ARPSTATE_ENABLED))); cnt++) ;

				useGW = 0;
		}

		if(cnt >= MAX_ARP_ENTRIES) {

			if(useGW)
				arpQueryID=arp_sendquery(gatewayIP);
			else
				arpQueryID=arp_sendquery(header.destIP);

			if(arpQueryID < MAX_ARP_ENTRIES) 
			{
				// Wait for ARP response
				unsigned int currTime = globalTimer;
				while((arpTable[arpQueryID].state != ARPSTATE_ENABLED) &&
					(globalTimer - currTime) < 40) ;

				// Host unavailable
				if(arpTable[arpQueryID].state != ARPSTATE_ENABLED)
					return;
			}
			else
				return;

			cnt = arpQueryID;
		}

		arpTable[cnt].lifeTime = 600;
		ne2k_send((char *)arpTable[cnt].MAC, ipBuf, msgLen + 20, PACKETTYPE_IP, 0);
	}
	else
		ne2k_send((char *)&broadcastMAC, ipBuf, msgLen + 20, PACKETTYPE_IP, 0);

}

/*
 * arp_sendAliveQuery(ip)
 * 
 * Generate an ARP message requesting our own IP address... This is a general
 * method to inform other network clients of a new machine.
 */

void arp_sendAliveQuery(char *ip)
{
	unsigned int cnt0;
	arpPacket newPacket;

	newPacket.lOPER = 1;
	newPacket.hOPER = 0;
	newPacket.lHTYPE = 1;
	newPacket.hHTYPE = 0;
	newPacket.lPTYPE = 0x00;
	newPacket.hPTYPE = 0x08;
	newPacket.HLEN = 6;
	newPacket.PLEN = 4;

	memcpy(newPacket.senderHWA, localMAC, 6);
	memcpy(newPacket.senderIP, ip, 4);
	memcpy(newPacket.receiverIP, ip, 4);

	for(cnt0=0;cnt0<6;cnt0++)
		newPacket.receiverHWA[cnt0]=0xFF;

	ne2k_send(newPacket.receiverHWA, (char *) &newPacket, 28, 0x806, 1);

}

/*
 * arp_sendquery(ip)
 * 
 * Generate an ARP message requesting MAC-address of an unknown host.
 */

unsigned int arp_sendquery(char *ip)
{
	arpPacket newPacket;
	unsigned int cnt, cnt0;

	/* Find a free ARP-entry */
	for(cnt = 0; (cnt < MAX_ARP_ENTRIES) && (arpTable[cnt].state !=
		ARPSTATE_DISABLED); cnt++) ;

	/* If we found one... */
	if(cnt < MAX_ARP_ENTRIES)
	{

		/* Initialize the packet */
		newPacket.lOPER = 1;
		newPacket.hOPER = 0;
		newPacket.lHTYPE = 1;
		newPacket.hHTYPE = 0;
		newPacket.lPTYPE = 0x00;
		newPacket.hPTYPE = 0x08;
		newPacket.HLEN = 6;
		newPacket.PLEN = 4;

		memcpy(newPacket.senderHWA, localMAC, 6);
		memcpy(newPacket.senderIP, localIP, 4);

		for(cnt0 = 0; cnt0 < 6; cnt0++)
			newPacket.receiverHWA[cnt0] = 0xFF;

		/* Copy the IP-address of the host */
		memcpy(newPacket.receiverIP, ip, 4);

		/* Mark that that we are waiting for an answer */
		arpTable[cnt].state = ARPSTATE_WAITING;

		for(cnt0 = 0; cnt0 < 4; cnt0++)
			arpTable[cnt].IP[cnt0]=ip[cnt0];

		/* Send the packet */
		ne2k_send(newPacket.receiverHWA,
			(char *) &newPacket, 28, 0x806, 1);

		return cnt;
	}

	/* No free ARP entries */
	return MAX_ARP_ENTRIES;
}

/*
 * arp_handle(packetData)
 * 
 * Handle an incoming ARP-packet. This function should be called after the
 * packet is recognised to be an ARP-packet.
 */

void arp_handle(etherPacket *packetData)
{
	arpPacket * arpPacketData = (arpPacket *) packetData->packetData;
	arpPacket newPacket;
	unsigned int cnt;

	/* ARP request message */
	if((arpPacketData->hOPER == 0) && (arpPacketData->lOPER == 1) && 
		!memcmp(arpPacketData->receiverIP, localIP, 4))
	{
		/* Initialize the packet */
		newPacket.lOPER = 2;
		newPacket.hOPER = 0;
		newPacket.lHTYPE =1;
		newPacket.hHTYPE =0;
		newPacket.lPTYPE=0x00;
		newPacket.hPTYPE=0x08;
		newPacket.HLEN=6;
		newPacket.PLEN=4;

		memcpy(newPacket.senderHWA, localMAC, 6);
		memcpy(newPacket.senderIP, localIP, 4);
		memcpy(newPacket.receiverHWA, arpPacketData->senderHWA, 6);
		memcpy(newPacket.receiverIP, arpPacketData->senderIP, 4);

		/* Reply to the request */
		ne2k_send(newPacket.receiverHWA, (char *) &newPacket, 28,
			PACKETTYPE_ARP, 0);
	}


	/* ARP response */
	if((arpPacketData->hOPER == 0) && (arpPacketData->lOPER == 2) &&
		!memcmp(arpPacketData->receiverIP, localIP, 4) &&
		!memcmp(arpPacketData->receiverHWA, localMAC, 6))
	{
		/* Retrieve the ARP entry */
		for(cnt = 0; ((cnt < MAX_ARP_ENTRIES) &&
			(memcmp(arpPacketData->senderIP, (char *)arpTable[cnt].IP, 4))); cnt++) ;

		/* Did we found the entry and is it pending? */
		if((cnt < MAX_ARP_ENTRIES) &&
			(arpTable[cnt].state != ARPSTATE_ENABLED)) {

			/* Yes. Copy the MAC-address and enable the ARP entry */
			memcpy((char *)arpTable[cnt].MAC, arpPacketData->senderHWA, 6);
			arpTable[cnt].state = ARPSTATE_ENABLED;
		}
	}
}


void ip_handle(etherPacket *packetData)
{
	ipHeader * header = (ipHeader *)packetData->packetData;
	//unsigned int checksum;
	unsigned int cnt;
	unsigned int originalChecksum;
	unsigned int checksum;

	/* Multiple part packets are ignored */
	if((header->flgFrgOffset[0] != 0x00 ||
		header->flgFrgOffset[1] != 0x00) &&
		!(header->flgFrgOffset[0] & 0xC0 )) {

		return;

}

	/* See if the sender is already in the ARP table */
	for(cnt = 0; ((cnt < MAX_ARP_ENTRIES) &&
		(memcmp(header->sourceIP, (char *)arpTable[cnt].IP, 4))); cnt++) ;

	/* If the sender is not */
	if((arpTable[cnt].state != ARPSTATE_ENABLED) || (cnt>=MAX_ARP_ENTRIES))
		/* Look for a free ARP table entry */
		for(cnt = 0; (cnt < MAX_ARP_ENTRIES) &&
			(arpTable[cnt].state != ARPSTATE_DISABLED); cnt++) ;

	/* If there's any sensible value in cnt (IP found, or there's space in the ARP table).. */
	if(cnt < MAX_ARP_ENTRIES) {
		/* Replace the current MAC address */
		memcpy((char *)arpTable[cnt].MAC, packetData->packetSender,6);
		memcpy((char *)arpTable[cnt].IP, header->sourceIP,4);
		arpTable[cnt].state=ARPSTATE_ENABLED;
		arpTable[cnt].lifeTime=600;
	}
	else
		/* If there was no free space in the ARP table, ignore the packet (there is no way to response for sender) */
		return;

	/* Check the checksum */
	originalChecksum = header->checksum[0] << 8 | header->checksum[1];
	header->checksum[0] = header->checksum[1] = 0x00;
	checksum = ip_calculateChecksum((char *)header, (header->verHLen & 0x0F) * 4);

	/* Checksum mismatch */
	if(originalChecksum != checksum)
		return;

	/* ICMP, UDP and TCP protocols are supported */
	switch(header->protocol)
	{
	case IPPACKETTYPE_ICMP:
		icmp_handle(packetData->packetData);
		break;
	case IPPACKETTYPE_UDP:
		udp_handle(packetData->packetData);
		break;
	case IPPACKETTYPE_TCP:
		tcp_handle(packetData->packetData);
		break;
	}

}

void packet_receive(etherPacket *packetData)
{
	unsigned int packetType = (packetData->packetType[0] << 8) | (packetData->packetType[1] & 0xFF);

	switch(packetType)
	{
	case PACKETTYPE_ARP:
		arp_handle(packetData);
		break;

	case PACKETTYPE_IP:
		ip_handle(packetData);
		break;
	}
}
