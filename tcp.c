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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ip.h"
#include "tcp.h"
#include "gtimer.h"
#include "config.h"
#include "fifo.h"

#define MAX(X,Y) ((X) > (Y) ? (X) : (Y))

/*
 * Reference to local IP address (needed while generating a TCP packet)
 */
extern unsigned char localIP[4];

/*
 * Some static structures...
 */

/* Each packet is constructed to this buffer before transmission */
static char packetBuf[TCP_TX_BUF_SIZE];

/* Pointer to the payload section in the packet */
static char *payloadBuf;
static unsigned int payloadBufLen;

/*
 * Indicates that tcp_sustain() is running (just a flag for the interrupt
 * routine)
 */
volatile static char sustainer_running;

/*
 * Sturcture to preserve information of TCP sockets
 */

tcpSocket sockets[MAX_TCP_SOCKETS];

/*
 * Prototypes of static functions
 */

static void increaseSeqNum(tcpSocket * socket, int len);
static void increaseAckNum(tcpSocket * socket, int len);
static void tcp_send(tcpSocket *socket, unsigned char flags,
						unsigned int len);

/*
 * tcp_validateSocket()
 *
 * Validate that the given socket is proper. Returns the index of the socket
 * if the socket is usable, -1 if not.
 */
static int tcp_validateSocket(tcpSocket * socket)
{	
	int i;
	for (i = 0; i < MAX_TCP_SOCKETS; i++) {
		if(&sockets[i] == socket)
			break;
	}

	if(&sockets[i] != socket)
			return -1;

	return i;
}

/*
 * stdio2socket(stream)
 *
 * Converts given FILE stream into a pointer to the socket structure
 */
static tcpSocket * stdio2socket(FILE *stream)
{
	int i;
	for (i = 0; i < MAX_TCP_SOCKETS; i++) {
		if(&sockets[i].stdio == stream)
			break;
	}

	if(&sockets[i].stdio != stream)
		return 0;

	return &sockets[i];
}

/*
 * tcp_getchar(stream)
 *
 * Get a character from the given stream. This function blocks until
 * a) a new character is available
 * b) timeout occurs
 * c) a new character is received
 */
static int tcp_getchar(FILE *stream)
{
	tcpSocket * socket = stdio2socket(stream);

	if(!socket)
		return 0;

	unsigned int tics = globalTimer + socket->streamTimeout;
	int len;

	while(socket->state == TCPSOCKETSTATE_ESTABLISHED) {
		
		if((len = fifo_length(&socket->strm.in))) {
			if(!socket->lastWindowSize &&
				(len < fifo_size(&socket->strm.in) * (1.0 - TCP_RX_BUF_MIN_SIZE)))
				tcp_send(socket, TCPFLAGS_ACK, 0);

			return fifo_getc(&socket->strm.in);
		}

		if(socket->streamTimeout && tics == globalTimer)
			return _FDEV_EOF;
	}

	return _FDEV_EOF;
}

/*
 * tcp_putchar(stream)
 *
 * Put a character to the send queue. This function blocks if there is no
 * sufficient space in the send buffer.
 */
static int tcp_putchar(char c, FILE *stream)
{
	tcpSocket * socket = stdio2socket(stream);

	if(!socket)
		return 0;

	if(socket->state != TCPSOCKETSTATE_ESTABLISHED)
		return 0;

	while(fifo_putc(&socket->strm.out, c)) ;

	return 0;
}

/*
 * tcp_setTimeout(socket, timeout)
 *
 * Set receive timeout for the socket.
 */
void tcp_setTimeout(tcpSocket * socket, unsigned int t)
{
	if(tcp_validateSocket(socket) < 0)
		return;

	socket->streamTimeout = t;
}

/*
 * tcp_initialise()
 *
 * This function initialises internal structures. In other words, this
 * function ensures that all sockets are in consistent state.
 */
void tcp_initialise(void)
{
	int i;
	for(i = 0; i < MAX_TCP_SOCKETS; i++) {
		sockets[i].state = TCPSOCKETSTATE_UNUSED;
		fdev_setup_stream(&sockets[i].stdio, tcp_putchar, tcp_getchar,
							_FDEV_SETUP_RW);

	}

	/* Initialize a pointer to payload section of each packet (this way we
	 * prevent having two buffers for the same data) */

	payloadBuf = packetBuf + 32;
	payloadBufLen = sizeof(packetBuf) - 32;
	sustainer_running = 0;
}

/*
 * tcp_reserveSocket(inBufSize, outBufSize)
 *
 * This function reserves a socket and initialises required stream objects.
 */
tcpSocket * tcp_reserveSocket(void * inBuf, void * outBuf, void * fsBuf,
								unsigned int inBufSize,
								unsigned int outBufSize)
{
	unsigned int i, j;

	// Find first free socket
	for(i = 0; i <	MAX_TCP_SOCKETS; i++) {
		if(sockets[i].state == TCPSOCKETSTATE_UNUSED)
			break;
	}

	// If a free socket is found...
	if(i >= MAX_TCP_SOCKETS)
		return 0;

	// Initialise it
	sockets[i].state = TCPSOCKETSTATE_UNKNOWN;
	sockets[i].ackState = 0;
	sockets[i].streamTimeout = 0;

	for(j = 0; j < 4; j++) {
		sockets[i].ackNum[j] = 0;
		sockets[i].seqNum[j] = 0;
		sockets[i].destIP[j] = 0;
	}

	// Initialize fifos
	fifo_initialize(&sockets[i].strm.in, inBufSize, inBuf);
	fifo_initialize(&sockets[i].strm.out, outBufSize, outBuf);
	fifo_initialize(&sockets[i].fsBuf, fsBuf?outBufSize:0, fsBuf);

	// And return address of the free socket for user
	return &sockets[i];

}

/*
 * tcp_releaseSocket(socket)
 *
 * This function frees a reserved TCP socket
 */
void tcp_releaseSocket(tcpSocket * socket)
{
	if(!socket) 
		return;

	socket->state = TCPSOCKETSTATE_UNUSED;
}

/*
 * tcp_listen(socket)
 *
 * Put socket into listening mode
 */
void tcp_listen(tcpSocket * socket)
{
	if(!socket)
		return;

	socket->state = TCPSOCKETSTATE_LISTEN;
}

/*
 * tcp_connect(socket)
 *
 * Ask socket object to connect. IP address, local port, remote port, etc.
 * must be initialised in application program.
 */
void tcp_connect(tcpSocket * socket)
{
	if(!socket)
		return;

	/* Send SYN message to the host */
	socket->state = TCPSOCKETSTATE_SYN_SENT;
	socket->ackState = TCP_RETRY_INTERVAL;
	socket->retryCounter = TCP_TOTAL_RETRIES;

	tcp_send(socket, TCPFLAGS_SYN, 0);

	/* Increase sequence number (SYN-message is considered as a one byte
	 * message) */

	increaseSeqNum(socket, 1);
}

/*
 * tcp_disconnect(socket)
 *
 * This function transmits TCP FIN packet to the active socket.
 */
void tcp_disconnect(tcpSocket *socket)
{
	if(!socket)
		return;

	/* Send FIN-message */
	socket->state=TCPSOCKETSTATE_FIN_WAIT_1;
	tcp_send(socket, TCPFLAGS_FIN | TCPFLAGS_ACK, 0);

	unsigned int tics = globalTimer + 100;

	while(globalTimer != tics && socket->state == TCPSOCKETSTATE_FIN_WAIT_1) ;
	socket->state = TCPSOCKETSTATE_UNKNOWN;

}
/*
 * tcp_flush(socket)
 *
 * Wait until all bytes in the socket have reached the destination
 */
void tcp_flush(tcpSocket *socket)
{
	if(!socket)
		return;

	while(fifo_length(&socket->strm.out)) ;
	
	unsigned int tics = globalTimer + 100;

	while((socket->ackState || socket->retryCounter) && globalTimer != tics &&
		socket->state == TCPSOCKETSTATE_ESTABLISHED) ;

}

/*
 * tcp_send(socket, flags, msg, len)
 *
 * This routine generates a valid TCP packet using the information given and
 * transmits the packet.
 */
static void tcp_send(tcpSocket *socket, unsigned char flags,
						unsigned int len)
{
	unsigned int checksum;
	
	tcpPseudoHeader * newPacketPH = (tcpPseudoHeader *)&packetBuf;
	tcpPacket * newPacket = (tcpPacket *)&packetBuf[12];

	/* Copy source and destination IP addresses */
	memcpy(newPacketPH->sourceIP, localIP, 4);
	memcpy(newPacketPH->destIP, socket->destIP, 4);

	/* Select protocol to be used */
	newPacketPH->protocol = IPPACKETTYPE_TCP;
	newPacketPH->zeroByte = 0x00;

	/* Calculate packet length */
	newPacketPH->pLen[0] = (len + 20) >> 8;
	newPacketPH->pLen[1] = (len + 20) & 0xFF;

	/* Copy source and destination port numbers */
	newPacket->lPort[0] = socket->localPort >> 8;
	newPacket->lPort[1] = socket->localPort & 0xFF;
	newPacket->dPort[0] = socket->remotePort >> 8;
	newPacket->dPort[1] = socket->remotePort & 0xFF;

	/* Header size is 5 * 4 (=20) bytes */
	newPacket->headerSize = 5 << 4;
	newPacket->checksum[0] = newPacket->checksum[1] = 0x00;

	/* Insert sequence and acknowledgement numbers */
	memcpy(newPacket->ackNum, socket->ackNum,4);
	memcpy(newPacket->seqNum, socket->seqNum,4);
	
	/* Receive window depends on receive buffer size */
	unsigned int windowSize = (socket->strm.in.size -
		fifo_length(&socket->strm.in));
	if(windowSize < TCP_RX_BUF_MIN_SIZE * fifo_size(&socket->strm.out))
		windowSize = 0;

	socket->lastWindowSize = windowSize;

	newPacket->receiveWindow[0] =
		windowSize >> 8; 
	newPacket->receiveWindow[1] =
		windowSize & 0xFF;

	/* Urgent packets are not supported */
	newPacket->urgent[0] = newPacket->urgent[1] = 0x00;

	/* Insert flags (SYN, FIN, ACK, etc.) */
	newPacket->codeBits = flags;

	/* Calculate checksum */
	checksum = ip_calculateChecksum(packetBuf, len + 12 + 20);

	/* Insert checksum and send */
	newPacket->checksum[0] = checksum >> 8;
	newPacket->checksum[1] = checksum & 0xFF;

	ip_send(socket->destIP, IPPACKETTYPE_TCP, &packetBuf[12], len + 20);
}

/*
 * increaseSeqNum(socket, len)
 *
 * Helper function for increasing the sequence packet number.
 */
static void increaseSeqNum(tcpSocket * socket, int len)
{
	unsigned int oldCnt0 = socket->seqNum[2] << 8 | socket->seqNum[3];
	unsigned int oldCnt1 = socket->seqNum[0] << 8 | socket->seqNum[1];

	if(oldCnt0+len<oldCnt0) oldCnt1++;
	oldCnt0+=len;

	socket->seqNum[0] = (char)(oldCnt1 >> 8);
	socket->seqNum[1] = (char)(oldCnt1 & 0xFF);

	socket->seqNum[2] = (char)(oldCnt0 >> 8);
	socket->seqNum[3] = (char)(oldCnt0 & 0xFF);
}

/*
 * increaseAckNum(socket, len)
 *
 * Helper functions for increasing the acknowledgement packet number.
 */
static void increaseAckNum(tcpSocket * socket, int len)
{
	unsigned int oldCnt0 = socket->ackNum[2] << 8 | socket->ackNum[3];
	unsigned int oldCnt1 = socket->ackNum[0] << 8 | socket->ackNum[1];

	if(oldCnt0+len<oldCnt0) oldCnt1++;
	oldCnt0+=len;

	socket->ackNum[0] = (char)(oldCnt1 >> 8);
	socket->ackNum[1] = (char)(oldCnt1 & 0xFF);

	socket->ackNum[2] = (char)(oldCnt0 >> 8);
	socket->ackNum[3] = (char)(oldCnt0 & 0xFF);
}

/*
 * tcp_sustain()
 *
 * This routine checks each active stream and delivers available data
 * forward. This function should be called from ip_handle.
 */
void tcp_sustain(void)
{
	unsigned int cnt, i;
	unsigned int fcount;

	if(sustainer_running)
		return;

	sustainer_running = 1;

	for(cnt=0; cnt<MAX_TCP_SOCKETS; cnt++) {
		
		/* Decrease acknowledgement number if it's positive */
		if(sockets[cnt].ackState)
			sockets[cnt].ackState--;

		switch(sockets[cnt].state) {
			case TCPSOCKETSTATE_SYN_SENT:
				if(!sockets[cnt].ackState) {
					if(sockets[cnt].retryCounter) {
						tcp_send(&sockets[cnt], TCPFLAGS_SYN, 0);
						sockets[cnt].ackState = TCP_RETRY_INTERVAL;
						sockets[cnt].retryCounter--;
					} else
						sockets[cnt].state = TCPSOCKETSTATE_UNKNOWN;
				}
				break;
			case TCPSOCKETSTATE_ESTABLISHED:

				// See if there's a packet that need to be revived
				if(fifo_size(&sockets[cnt].fsBuf) &&
					fifo_length(&sockets[cnt].fsBuf) &&
					!sockets[cnt].ackState) {

					if(sockets[cnt].retryCounter) {
						
						/* ACK counter is null and there's still data..
						 * try to send it again */

						int len = fifo_length(&sockets[cnt].fsBuf);

						for(i = 0; (i < len) &&
							(i < payloadBufLen); i++)

							payloadBuf[i] = fifo_getc(&sockets[cnt].fsBuf);

						/* Send the data. Note that the sequence number is not
						 * increased */
						tcp_send(&sockets[cnt], TCPFLAGS_ACK | TCPFLAGS_PSH,
							i);

						/* Decrease retry counter */
						sockets[cnt].retryCounter--;
					} else
						sockets[cnt].state = TCPSOCKETSTATE_UNKNOWN;

				}
				else if((fcount = fifo_length(&sockets[cnt].strm.out))) {

					/* We can proceed with new data */
					for(i = 0; i < fcount &&
						i < payloadBufLen; i++) {

						payloadBuf[i] = fifo_getc(&sockets[cnt].strm.out);

						if(fifo_size(&sockets[cnt].fsBuf))
							fifo_putc(&sockets[cnt].fsBuf, payloadBuf[i]);
					}

					/* Send it and increase sequence number */
					tcp_send(&sockets[cnt], TCPFLAGS_ACK | TCPFLAGS_PSH,
						i);

					increaseSeqNum(&sockets[cnt], i);
					sockets[cnt].ackState = TCP_RETRY_INTERVAL;
					sockets[cnt].retryCounter = TCP_TOTAL_RETRIES;
				}

				break;
		}
	}

	sustainer_running = 0;
}



/*
 * tcp_handle(packetData)
 *
 * This routine handles incoming TCP packets. The routine should be called
 * from the IP packet handler.
 */
void tcp_handle(void *packetData)
{
	unsigned int cnt = 0;
	int i, send;

	/* Get IP-packet and TCP-packet structures */

	ipHeader *header = packetData;
	tcpPacket * packet = (void *)(((unsigned int)packetData) +
		((header->verHLen & 0x0F) * 4));

	/* Calculate some useful values from the packet */
	
	unsigned int dataCount = ((header->tLen[0] << 8) | header->tLen[1]) -
		((header->verHLen & 0x0F) * 4) - (packet->headerSize >> 4) * 4;
	unsigned int remotePort = (int)(packet->lPort[0] << 8 | packet->lPort[1]);
	unsigned int localPort = ((packet->dPort[0] << 8) | packet->dPort[1]);

	send = 0;

	for(cnt = 0; cnt < MAX_TCP_SOCKETS; cnt++)
	{
		/*
		 * SYN packet has been sent and we're waiting for a ackowledgement
		 * packet
		 */
		if (sockets[cnt].state == TCPSOCKETSTATE_SYN_SENT &&
			sockets[cnt].localPort ==
			((packet->dPort[0] << 8) | packet->dPort[1])) {

			if(packet->codeBits & TCPFLAGS_SYN &&
				packet->codeBits & TCPFLAGS_ACK) {

				memcpy(sockets[cnt].ackNum, packet->seqNum, 4);
				increaseAckNum(&sockets[cnt], 1 + dataCount);
				sockets[cnt].state = TCPSOCKETSTATE_ESTABLISHED;
				tcp_send(&sockets[cnt], TCPFLAGS_ACK, 0);
				fifo_reset(&sockets[cnt].strm.out);
				fifo_reset(&sockets[cnt].strm.in);
				fifo_reset(&sockets[cnt].fsBuf);
			}

			break;
		}

		/* The socket is in listening mode and SYN packet is received */
		if (sockets[cnt].state == TCPSOCKETSTATE_LISTEN &&
			sockets[cnt].localPort ==
			((packet->dPort[0] << 8) | packet->dPort[1])) {

			if(packet->codeBits & TCPFLAGS_SYN) {
				sockets[cnt].remotePort = remotePort;
				memcpy(sockets[cnt].destIP, header->sourceIP, 4);
				memcpy(sockets[cnt].ackNum, packet->seqNum, 4);
				increaseAckNum(&sockets[cnt], 1 + dataCount);
				tcp_send(&sockets[cnt], TCPFLAGS_SYN | TCPFLAGS_ACK, 0);
				increaseSeqNum(&sockets[cnt], 1 + dataCount);
				sockets[cnt].state = TCPSOCKETSTATE_ESTABLISHED;
				fifo_reset(&sockets[cnt].strm.out);
				fifo_reset(&sockets[cnt].strm.in);
				fifo_reset(&sockets[cnt].fsBuf);

				/* Packet is valid. Connection established. :) */
			}

			break;
		}
		
		/*
		 * Is the the socket is used in some way (ports and IP addresses
		 * match)?
		 */
		if(!memcmp(sockets[cnt].destIP, header->sourceIP, 4) &&
			sockets[cnt].localPort == localPort &&
			sockets[cnt].remotePort == remotePort) {

			/* Is the connection established already? */
			if(sockets[cnt].state == TCPSOCKETSTATE_ESTABLISHED) {
				/* Check for ACK packets */
				if(packet->codeBits & TCPFLAGS_ACK) {
					if(!memcmp(sockets[cnt].seqNum, packet->ackNum, 4)) {
						sockets[cnt].ackState = 0;
						sockets[cnt].retryCounter = 0;
						fifo_reset(&sockets[cnt].fsBuf);
					}
				}

				/* Check for SYN packets */
				if(packet->codeBits & TCPFLAGS_SYN) {
					memcpy(sockets[cnt].ackNum, packet->seqNum, 4);
					increaseAckNum(&sockets[cnt], 1 + dataCount);
					send = 1;
				}

				/* Copy data directly to the receive buffer. NOTE! We do not
				 * wait for PUSH before doing this */
				if(dataCount) {

					memcpy(sockets[cnt].ackNum, packet->seqNum, 4);
					increaseAckNum(&sockets[cnt], dataCount);
					send = 1;

					/* Copy the data into fifo */
					for(i = 0; i < dataCount; i++)
						fifo_putc(&sockets[cnt].strm.in, ((char *)packet)
							[i + (packet->headerSize >> 4) * 4]);

				}

				/* Check for disconnect packets */
				if(packet->codeBits & TCPFLAGS_FIN) {
					memcpy(sockets[cnt].ackNum, packet->seqNum, 4);
					increaseAckNum(&sockets[cnt], 1 + dataCount);
					tcp_send(&sockets[cnt], TCPFLAGS_ACK | TCPFLAGS_FIN, 0);
					sockets[cnt].state = TCPSOCKETSTATE_UNKNOWN;
				}

				/* We should send acknowledgement to the sender */
				if(send)
					tcp_send(&sockets[cnt], TCPFLAGS_ACK, 0);

				break;
			}

			if(sockets[cnt].state == TCPSOCKETSTATE_FIN_WAIT_1) {

				if(packet->codeBits & TCPFLAGS_FIN) {
					memcpy(sockets[cnt].ackNum, packet->seqNum, 4);
					increaseAckNum(&sockets[cnt], 1 + dataCount);
					tcp_send(&sockets[cnt], TCPFLAGS_ACK, 0);
					sockets[cnt].state = TCPSOCKETSTATE_UNKNOWN;

					break;
				}
			}
		}
	}
		
}
