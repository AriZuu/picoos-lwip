/* 
 * Copyright (c) 2001-2003 Swedish Institute of Computer Science. 
 * Copyright (c) 2014, Ari Suutari <ari@stonepile.fi>.
 * All rights reserved.  
 *  
 * Redistribution and use in source and binary forms, with or without modification,  
 * are permitted provided that the following conditions are met: 
 * 
 * 1. Redistributions of source code must retain the above copyright notice, 
 *    this list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright notice, 
 *    this list of conditions and the following disclaimer in the documentation 
 *    and/or other materials provided with the distribution. 
 * 3. The name of the author may not be used to endorse or promote products 
 *    derived from this software without specific prior written permission.  
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED  
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF  
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT  
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,  
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT  
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS  
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN  
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING  
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY  
 * OF SUCH DAMAGE. 
 * 
 * This file is part of the lwIP TCP/IP stack. 
 *  
 * Author: Adam Dunkels <adam@sics.se> 
 * 
 */ 

#ifndef __CC_H__ 
#define __CC_H__ 

#include <picoos.h>
#include <stdint.h>

#if __GNUC__ >= 5
#include <sys/types.h>
#define LWIP_TIMEVAL_PRIVATE 0
#endif

/* Types based on stdint.h */

typedef uint8_t            u8_t; 
typedef int8_t             s8_t; 
typedef uint16_t           u16_t; 
typedef int16_t            s16_t; 
typedef uint32_t           u32_t; 
typedef int32_t            s32_t; 
typedef uintptr_t          mem_ptr_t; 

/* Define (sn)printf formatters for these lwIP types */

#define U16_F "hu"
#define S16_F "hd"
#define X16_F "hx"
#define U32_F "lu"
#define S32_F "ld"
#define X32_F "lx"
#define SZT_F "uz"

#ifndef BYTE_ORDER
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define BYTE_ORDER LITTLE_ENDIAN
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define BYTE_ORDER BIG_ENDIAN
#else
#error Unknown byte order
#endif
#endif

#define LWIP_PROVIDE_ERRNO

#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_STRUCT __attribute__ ((__packed__))
#define PACK_STRUCT_END
#define PACK_STRUCT_FIELD(fld) fld

int sys_random(void);
#define LWIP_RAND() sys_random()

#ifdef LWIP_DEBUG

void assert_printf(char *msg, int line, char *file);

#define LWIP_PLATFORM_DIAG(vars) nosPrintf vars
#define LWIP_PLATFORM_ASSERT(x) do {nosPrintf("Assertion \"%s\" failed at line %d in %s\n", \
                                     x, __LINE__, __FILE__); } while(1)
#else
#define LWIP_PLATFORM_DIAG(msg) { ; }
#define LWIP_PLATFORM_ASSERT(flag) { ; }
#endif 

#endif /* __CC_H__ */ 
