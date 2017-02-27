/*****************************************************************************
 * dhcp.c
 * Arto Meriläinen
 * 2010	- 2011
 *****************************************************************************/

#include <stdio.h>
#include <string.h>

#include "ip.h"
#include "udp.h"
#include "dhcp.h"

/*****************************************************************************
 * IP settings are placed directly to the variables.
 *****************************************************************************/

extern unsigned char localIP[4];
extern unsigned char gatewayIP[4];
extern unsigned char netmask[4];

extern unsigned char localMAC[4];

extern unsigned int globalTimer;

/*****************************************************************************
 * Protocol specific stuff...
 *****************************************************************************/

static const char transactionID[] = {0x3D, 0x16, 0x3F, 0xEC};
static const char magicCookie[] = {0x63, 0x82, 0x53, 0x63};

/*****************************************************************************
 * Hard coded DHCP messages are defined here
 *****************************************************************************/

static char dhcpDiscoveryMsg[] =
{0x35, 1, 1,
0x74, 1, 1,
0x3D, 7, 1, 0, 0, 0, 0, 0, 0,
0x0C, 3, 'n', 't', 'k',
0x3C, 8, 'N', 'T', 'K', 'C', ' ', '0', '.', '1',
0x37, 11, 0x01, 0x0F, 0x03, 0x06, 0x2C, 0x2E, 0x2F, 0x1F, 0x21, 0xF9, 0x2B,
0xFF};

static char dhcpRequestMsg[] =
{0x35, 1, 3,
0x3D, 7, 1, 0, 0, 0, 0, 0, 0,
0x0C, 3, 'n', 't', 'k',
0x3C, 8, 'N', 'T', 'K', 'C', ' ', '0', '.', '1',
0x37, 11, 0x01, 0x0F, 0x03, 0x06, 0x2C, 0x2E, 0x2F, 0x1F, 0x21, 0xF9, 0x2B,
0xFF};

/** We use broadcast address for DHCP server */

static const unsigned char dhcpServer[] = {255, 255, 255, 255};

/*****************************************************************************
 * Prototypes of static functions
 *****************************************************************************/

static char * getParameter(char * dhcpOptions, int maxLength,
	int option);

/*****************************************************************************
 * dhcp_retrieveIP()
 *
 * Retrieve IP-settings using DHCP server
 *****************************************************************************/

unsigned int dhcp_retrieveIP(void)
{
	/** Initialise DHCP request message*/
	char buf[256];
	dhcpMessage * transmittedMessage = buf;
	dhcpMessage * receivedMessage;

	char temporary_localIP[4];
	char temporary_netmask[4];
	char gatewayAddr[4];
	char temporaryDhcpServer[4];

	char * temporary_netmask_ptr;
	char * gatewayAddr_ptr;
	char * temporaryDhcpServer_ptr;

	char udpData[256];

	udpSocket * socket = udp_register(68, udpData, 256);
	unsigned int tic0 = 0, tic1 = 0;
	unsigned int dhcpOptionLength;

	/** Create a dhcp discover message */
	transmittedMessage->opCode = 1;
	transmittedMessage->hardwareType = 1;
	transmittedMessage->hardwareAddressLength = 6;
	memcpy(transmittedMessage->transactionID, transactionID, 4);
	memcpy(transmittedMessage->clientHardwareAddress, localMAC, 6);
	memcpy(transmittedMessage->magicCookie, magicCookie, 4);
	memcpy(&dhcpDiscoveryMsg[9], localMAC, 6);
	memcpy((unsigned char *)(transmittedMessage + 1), dhcpDiscoveryMsg,
		sizeof(dhcpDiscoveryMsg));

	/** Clear IP address */
	localIP[0] = 0x00;
	localIP[1] = 0x00;
	localIP[2] = 0x00;
	localIP[3] = 0x00;

	/** Make sure that the UDP socket is listening */
	udp_reregister(socket);
	udp_send((char *)dhcpServer, 68, 67, (char *)transmittedMessage,
		(sizeof(dhcpMessage) + sizeof(dhcpDiscoveryMsg)) > 308 ?
		sizeof(dhcpMessage) + sizeof(dhcpDiscoveryMsg) : 300);

	/** tic0 is used to determine timeout */

	tic0 = tic1 = globalTimer;
	
	/** Loop until answer is recieved... or timeout occurs */
	while(socket->state != SOCKETSTATE_ESTABLISHED) {

		/** UDP is unreliable (as if), thus, repeat sending the request */
		if(globalTimer - tic1 > 100) {
			
			/** Increase transaction ID (just in case...) */

			transmittedMessage->transactionID[0]++;
			udp_send((char *)dhcpServer, 68, 67, (char *)transmittedMessage,
				(sizeof(dhcpMessage) + sizeof(dhcpDiscoveryMsg)) > 308 ?
				sizeof(dhcpMessage) + sizeof(dhcpDiscoveryMsg) : 300);
			tic1 = globalTimer;
		}

		/** Timeout? */
		if(globalTimer - tic0 > 500) {
			break;
		}
	}

	/** Timeout occured... return with an error code */

	if(socket->state != SOCKETSTATE_ESTABLISHED) {
		udp_disconnect(socket);
		return 1;
	}

	/** We have a message \o/ Parse it... */
	receivedMessage = (dhcpMessage *)socket->dbuf;
	dhcpOptionLength = socket->dLen - sizeof(dhcpMessage);

	/** Make sure that the packet type is DHCP discover */
	if(!getParameter((char *)(socket->dbuf + sizeof(dhcpMessage)),
		dhcpOptionLength, 0x35)) {

		udp_disconnect(socket);

		return 2;
	}

	/** Get pointer to the network mask */
	if(!(temporary_netmask_ptr =
		getParameter((char *)(socket->dbuf + sizeof(dhcpMessage)),
		dhcpOptionLength, 0x01))) {

		udp_disconnect(socket);
		return 3;
	}

	/** Get pointer to the gateway IP */
	if(!(gatewayAddr_ptr =
		getParameter((char *)(socket->dbuf + sizeof(dhcpMessage)),
		dhcpOptionLength, 0x03))) {

		udp_disconnect(socket);
		return 4;
	}

	/** Get the IP address of the DHCP server */
	if(!(temporaryDhcpServer_ptr =
		getParameter((char *)(socket->dbuf + sizeof(dhcpMessage)),
		dhcpOptionLength, 0x36))) {

		udp_disconnect(socket);
		return 5;
	}

	/** Extract the actual data from the buffer */
	memcpy(temporary_localIP, receivedMessage->yourIP, 4);
	memcpy(temporaryDhcpServer, temporaryDhcpServer_ptr + 2, 4);
	memcpy(gatewayAddr, gatewayAddr_ptr + 2, 4);
	memcpy(temporary_netmask, temporary_netmask_ptr + 2, 4);

	/** Generate a new message */
	memcpy(transmittedMessage->serverIP, temporaryDhcpServer, 4);
	memcpy(transmittedMessage->yourIP, temporary_localIP, 4);

	/** Inform other network clients of our existence */
	arp_sendAliveQuery(temporary_localIP);

	/** Request IP address from the DHCP server */
	memcpy(&dhcpRequestMsg[6], localMAC, 6);
	memcpy((unsigned char *)(transmittedMessage + 1), dhcpRequestMsg,
		sizeof(dhcpRequestMsg));

	/** Put the UDP socket back to listening mode */
	udp_reregister(socket);

	/** Send an UDP message */
	udp_send((char *)dhcpServer, 68, 67, (char *)transmittedMessage,
		(sizeof(dhcpMessage) + sizeof(dhcpRequestMsg)) > 308 ?
		sizeof(dhcpMessage) + sizeof(dhcpRequestMsg) : 300);

	/** Update counter variables */
	tic0 = tic1 = globalTimer;
	while(socket->state != SOCKETSTATE_ESTABLISHED) {

		/** UDP is unreliable (as if), thus, repeat sending the request */
		
		if(globalTimer - tic1 > 100) {
			udp_send((char *)dhcpServer, 68, 67, (char *)transmittedMessage,
				(sizeof(dhcpMessage) + sizeof(dhcpRequestMsg)) > 308 ?
				sizeof(dhcpMessage) + sizeof(dhcpRequestMsg) : 300);

			tic1 = globalTimer;
		}

		/** Timeout? */
		if(globalTimer - tic0 > 500) {
			break;
		}
	}

	/** Did we get a response? */
	if(socket->state != SOCKETSTATE_ESTABLISHED) {

		udp_disconnect(socket);
		return 1;
	}

	/** Update the local IP address */

	memcpy(localIP, temporary_localIP, 4);
	memcpy(gatewayIP, gatewayAddr, 4);
	memcpy(netmask, temporary_netmask, 4);

	/** Buffers are not needed anymore */
	udp_disconnect(socket);

	return 0;
}

/*****************************************************************************
 * getParameter(dhcpOptions, maxLength, option)
 *
 * A helper function to extract data from a DHCP message.
 *****************************************************************************/

static char * getParameter(char * dhcpOptions, int maxLength, int option)
{
	int i = 0;
	while(i < maxLength) {
		if(dhcpOptions[i] == option)
			return &dhcpOptions[i];
		i += dhcpOptions[i + 1] + 2;
	}

	return (char *)0;
}
