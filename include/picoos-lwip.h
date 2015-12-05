/*
 * Copyright (c) 2015, Ari Suutari <ari@stonepile.fi>.
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote
 *     products derived from this software without specific prior written
 *     permission. 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT,  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#include <picoos-u.h>

#include <lwip/sockets.h>
#include <lwip/netdb.h>

#if !LWIP_COMPAT_SOCKETS

void netInit(void);
int netLwIP_FD(int s);

int accept(int s, struct sockaddr *addr, socklen_t *addrlen);
int socket(int domain, int type, int protocol);

#define bind(s, name, namelen)                         lwip_bind(netLwIP_FD(s), name, namelen)
#define shutdown(s, how)                               lwip_shutdown(netLwIP_FD(s), how)
#define getpeername(s, name, namelen)                  lwip_getpeername(netLwIP_FD(s), name, namelen)
#define getsockname(s, name, namelen)                  lwip_getsockname (netLwIP_FD(s), name, namelen)
#define getsockopt (s, level, optname, optval, optlen) lwip_getsockopt (netLwIP_FD(s), level, optname, optval, optlen)
#define setsockopt(s, level, optname, optval, optlen)  lwip_setsockopt(netLwIP_FD(s), level, optname, optval, optlen)
#define connect(s, name, namelen)                      lwip_connect(netLwIP_FD(s), name, namelen)
#define listen(s, backlog)                             lwip_listen(netLwIP_FD(s), backlog)
#define recv(s, mem, len, flags)                       lwip_recv(netLwIP_FD(s), mem, len, flags)
#define recvfrom(s, mem, len, flags, from, fromlen)    lwip_recvfrom(netLwIP_FD(s), mem, len, flags, from, fromlen)
#define send(s, dataptr, size, flags)                  lwip_send(netLwIP_FD(s), dataptr, size, flags)
#define sendmsg(s, message, flags)                     lwip_sendmsg(netLwIP_FD(s), message, flags)
#define sendto(s, dataptr, size, flags, to, tolen)     lwip_sendto(netLwIP_FD(s), dataptr, size, flags, to, tolen)
#define writev(s, iov, iovcnt)                         lwip_writev(netLwIP_FD(s), iov, iovcnt)
#define closesocket(s)                                 lwip_close(netLwIP_FD(s))

#define gethostbyname(name) lwip_gethostbyname(name)
#define gethostbyname_r(name, ret, buf, buflen, result, h_errnop) \
       lwip_gethostbyname_r(name, ret, buf, buflen, result, h_errnop)
#define freeaddrinfo(addrinfo) lwip_freeaddrinfo(addrinfo)
#define getaddrinfo(nodname, servname, hints, res) \
       lwip_getaddrinfo(nodname, servname, hints, res)

#endif

#ifdef __cplusplus
} // extern "C"
#endif /* __cplusplus */
