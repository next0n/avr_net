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

#ifndef FILEOPS_H
#define FILEOPS_H

#define include_data(data, data_end, filename)				\
	__asm__ (												\
		/* Use program memory for the variable */			\
    	".pushsection .progmem \n"  	  					\
    		".global " #data "\n"   	 					\
    		".type " #data ", @object \n"					\
															\
			/* Mark start of the variable */				\
    		#data ": \n"									\
															\
			/* Include the data */							\
    		".incbin \"" filename "\" \n"					\
															\
			/* Calculate size of the variable */			\
    		".size " #data ", .- " #data "\n"				\
    		".align 2 \n"									\
															\
			/* Create a variable to denote end of data */	\
			".global " #data_end "\n"						\
			".type " #data_end ", @object \n"				\
			#data_end ":\n"									\
    	".popsection"										\
															\
);															\
															\
/* Make the variable (and its ending) visible in C-code */	\
extern char data[] __attribute__((section(".progmem")));	\
extern char data_end[] __attribute__((section(".progmem")))

#endif
