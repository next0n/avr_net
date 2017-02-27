/*
 * Copyright (c) 2011-2017, Arto Merilainen (arto.merilainen@gmail.com)
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
#include "ip.h"
#include "uart.h"
#include "tcp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <avr/pgmspace.h>
#include <avr/io.h>
#include <util/delay.h>

/*
 * Few global variables/buffers
 */

/* TCP buffers */
static char inBuf[100], outBuf[100], fsBuf[100], lineBuf[100];
/* URI buffer */
static char filename[64];
/* Request type buffer */
static char requestType[16];
/* TCP socket */
static tcpSocket * socket;
/* HTTP request version */
static unsigned int v1, v2;
/* Pointer to the files in the filesystem */
static char ** httpd_files;

/*
 * httpd_transmit_ok_header()
 *
 * Output found header to the standard output.
 */
void httpd_transmit_ok_header(void)
{
	printf("HTTP/%1u.%1u 200 Found\n"
			"Server: AVR Web Server\n"
			"Last-Modified: Wed, 13 Jul 2011 23:22:34 GMT\n"
			"Connection: close\n", v1, v2);
}

/*
 * httpd_transmit_file(filename)
 *
 * Read the file from the flash and output it to the stdio. Return value 0 is
 * returned on success, value -1 is returned if the file is not found.
 */
int httpd_transmit_file(char *filename)
{
	/* Redirect automatically to index file */
	if(!strcmp(filename, "/"))
		strcpy(filename, "/index.html");

	/* Find the file from the "storage" */
	int i;
	for(i = 0; httpd_files[i]; i += 3)
		if(!strcmp(filename, httpd_files[i]))
			break;

	/* Did we find the file? */
	if(!httpd_files[i])
		return -1;
	
	/* Read the file from the flash */
	char chr;
	PGM_P ptr = (PGM_P) httpd_files[i + 1];
	while(ptr != httpd_files[i + 2]) {
		chr = pgm_read_byte(ptr++);
		fputc((const char )chr, &socket->stdio);
	}

	return 0;
}

/*
 * httpd_get_uri_param(uri, parameter, buf, maxlen)
 *
 * Gets the value of the parameter from the given URI. The filename is put to
 * the buf. If the parameter is not found, an error code -1 is returned. If
 * the parameter in the uri is not valid, error code -2 is returned. If the
 * buffer does not have sufficient amount of space available, an error -3 is
 * returned. On success, the length of the parameter value is returned.
 */
int httpd_get_uri_param(char * uri, char *param, char *buf, int maxlen)
{
	char * startplace = uri;

	/* Find the parameter from the uri */
	while((startplace = strstr(startplace, param))) {

		if((startplace[-1] == '?' || startplace[-1] == '&') &&
			startplace[strlen(param)] == '=') {

			break;
		}
		
		startplace += strlen(param);
	}

	/* Was the parameter found? */
	if(!startplace)
		return -1;

	/* Move pointer to the start of the parameter value */
	startplace += strlen(param) + 1;

	/* Find the end of the parameter */
	char * endplace = strchr(startplace, '&');
	if(!endplace)
		 endplace = strchr(startplace, '\0');
	if(!endplace)
		return -2;

	/* Calculate the length of the value */
	int len = endplace - startplace;

	/* Make sure the buffer is long enough */
	if(len >= maxlen)
		return -3;

	/* Copy the value into the buffer and add ending mark */
	memcpy(buf, startplace, len);
	buf[len] = '\0';

	return len;
}

/*
 * httpd_get_uri_filename(uri, buf, maxlen)
 *
 * Gets a filename from the given URI. The filename is put to the buf. If the
 * filename is not valid, an error code -2 is returned. If the buffer does not
 * have sufficient amount of space available, an error -3 is returned. On
 * success, the length of the filename is returned.
 */
int httpd_get_uri_filename(char * uri, char *buf, int maxlen)
{

	/* Find the end of the filename */
	char * endplace = strchr(uri, '?');
	if(!endplace)
		endplace = strchr(uri, '\0');
	if(!endplace)
		return -2;

	/* Calculate the size of the filename */
	int len = endplace - uri;

	/* Is the buffer large enough? */
	if(len >= maxlen)
		return -3;

	/* Copy the filename into the buffer */
	memcpy(buf, uri, len);
	buf[len] = '\0';

	return len;
}

/*
 * httpd_start(port, files, callback)
 *
 * Start HTTPD server. The callback function is called when (any) valid HTTPD
 * request is made. If the callback is unable to handle the request, the
 * server tries answering to the request.
 *
 * Currently, only GET requests can be handled automatically. The handler
 * try to find the requested file from the flash (pointer to the structure
 * given in "files" variable).
 */
void httpd_start(unsigned int port, char * files[],
				 int (*callback)(char *, char *))
{

	/* Set the local variable */
	httpd_files = files;

	/* Reserve a socket */
	socket = tcp_reserveSocket(inBuf, outBuf, fsBuf,
								sizeof(inBuf), sizeof(outBuf));

	/* Change stdio (this way printf works also in callback-function) */
	stdin = stdout = &socket->stdio;
	socket->localPort = port;

	while(1) {
		tcp_listen(socket);

		/* Wait until the socket gets connected */
		while(socket->state != TCPSOCKETSTATE_ESTABLISHED) ;

		/* Give some time to enter the command */
		tcp_setTimeout(socket, 1000);

		/* Get the first line (..which is the actual request) */
		if(!fgets(lineBuf, sizeof(lineBuf), stdin)) {
			printf("HTTP/%1u.%1u 400 Bad request\n\n", v1, v2);
			tcp_flush(socket);
			tcp_disconnect(socket);
			continue;
		}

		/* Clean up the received line */
		char * idx;
		if((idx = strchr(lineBuf, '\n')))
			*idx = '\0';
		if((idx = strchr(lineBuf, '\r')))
			*idx = '\0';
		
		/* Parse the line */
		unsigned int params;
		v1 = v2 = 1;
		params = sscanf(lineBuf, "%s %s HTTP/%u.%u", (char *)&requestType,
			(char *)&filename, &v1, &v2);

		tcp_setTimeout(socket, 500);
		while(fgets(lineBuf, sizeof(lineBuf), stdin)) {

			if((idx = strchr(lineBuf, '\n')))
				*idx = '\0';
			if((idx = strchr(lineBuf, '\r')))
				*idx = '\0';

			if(strlen(lineBuf) == 0)
				break;
		}
		
		if(params < 2) {
			printf("HTTP/%1u.%1u 400 Bad request\n\n", v1, v2);
			tcp_flush(socket);
			tcp_disconnect(socket);
			continue;
		}

		if(callback) {
			int retval = callback(requestType, filename);
			if(!retval) {
				tcp_flush(socket);
				tcp_disconnect(socket);
				continue;			
			} else if(retval == 1) {
				tcp_flush(socket);
				tcp_disconnect(socket);
				_delay_ms(100);
				asm("jmp 0");
			}
		}

		if(strcmp(requestType, "GET")) {
			printf("HTTP/%1u.%1u 501 Not implemented\n\n", v1, v2);
			tcp_flush(socket);
			tcp_disconnect(socket);
			continue;
		}

		/* Redirect automatically to index file */
		if(!strcmp(filename, "/"))
			strcpy(filename, "/index.html");

		/* Write log */
		fprintf(&uart_stdio, "%s: Requested page %s\n", requestType, filename);
		
		/* Find the file from the "storage" */
		int i;
		for(i = 0; httpd_files[i]; i += 3)
			if(!strcmp(filename, httpd_files[i]))
				break;

		/* Did we find the file? */
		if(!httpd_files[i]) {

			/* Nope. Close the connection */
			printf("HTTP/%1u.%1u 404 Not Found\n\n", v1, v2);
			tcp_flush(socket);
			tcp_disconnect(socket);
			continue;
		}

		/* Yes. Send header */
		httpd_transmit_ok_header();
		printf("\n");
		
		/* Read the file from the flash */
		char chr;
		PGM_P ptr = (PGM_P) httpd_files[i + 1];
		while(ptr != httpd_files[i + 2]) {
			chr = pgm_read_byte(ptr++);
			fputc((const char )chr, &socket->stdio);
		}

		/* Close the connection */
		tcp_flush(socket);
		tcp_disconnect(socket);
		
	}
}
