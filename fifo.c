/*
 * Copyright (c) 2006-2017, Arto Merilainen (arto.merilainen@gmail.com)
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

#include "fifo.h"

/*
 * fifo_initialize(fout, fsizer, faddr)
 *
 * Initialise fifo fout using buffer at faddr with size fsize.
 */
void fifo_initialize(fifo *fout, int fsize, char *faddr)
{
	fout->addr = faddr;
	fout->size = fsize;
	fout->writePtr = 0;
	fout->readPtr = 0;
	fout->lock = 0;
}

/*
 * fifo_reset(fout)
 *
 * Reset a fifo
 */
void fifo_reset(fifo *fout)
{
	fout->writePtr = 0;
	fout->readPtr = 0;
	fout->lock = 0;
}

/*
 * fifo_putc(chr, fout)
 *
 * Put a char into given fifo
 */
int fifo_putc(fifo *fout, char chr)
{
	int newWritePtr;

	if(!fout)
		return -1;

	newWritePtr = (fout->writePtr + 1) % fout->size;
	
	if(newWritePtr != fout->readPtr) {
		fout->addr[fout->writePtr] = chr;
		fout->writePtr = newWritePtr;
		return 0;
	}
	return -1;

}

/*
 * fifo_getc(fin)
 *
 * Wait for char from given fifo. This function should
 * NEVER be called while ints are disabled as otherwise
 * we are in deadlock.
 */
char fifo_getc(fifo *fin)
{
	char chr;

	if(!fin)
		return 0;

	/* Wait for data... */
	while(fin->writePtr == fin->readPtr) ;
	
	/* Get the data */
	chr = fin->addr[fin->readPtr];

	/* Update the read pointer */
	fin->readPtr = (fin->readPtr + 1) % fin->size;
	
	return chr;
}

/*
 * fifo_memchr(fin, chr)
 *
 * Checks whether a given character is in the fifo
 */
unsigned int fifo_memchr(fifo *fin, char chr)
{
	int ptr;
	int bytesAvailable;

	if(!fin)
		return 0;

	for(ptr = fin->readPtr; ptr != fin->writePtr; ptr = (ptr + 1) % fin->size) {
		if(fin->addr[ptr] == chr) {
			bytesAvailable = ptr - fin->readPtr;

			if(bytesAvailable >= 0)
				return bytesAvailable;
			else
				return bytesAvailable + fin->size;
		}
	}

	return 0;
}

/*
 * fifo_length(fin)
 *
 * Returns the number of bytes available in the fifo
 */
unsigned int fifo_length(fifo *fin)
{
	int bytesAvailable = fin->writePtr - fin->readPtr;

	if(!fin)
		return 0;

	if(bytesAvailable >= 0)
		return bytesAvailable;
	else
		return bytesAvailable + fin->size;
}

/*
 * fifo_size(fin)
 *
 * Returns the size of the fifo
 */
unsigned int fifo_size(fifo *fin)
{
	return fin->size;
}
