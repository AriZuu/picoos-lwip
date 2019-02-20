#
# Copyright (c) 2019, Ari Suutari <ari@stonepile.fi>.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#  1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#  3. The name of the author may not be used to endorse or promote
#     products derived from this software without specific prior written
#     permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
# INDIRECT,  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
# STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
# OF THE POSSIBILITY OF SUCH DAMAGE.

include_guard(GLOBAL)

set(LWIPDIR lwip/src)
include(${CMAKE_CURRENT_LIST_DIR}/Filelists.cmake)
set(SRC
    sys_arch.c
    sockets.c
    apps/dhcps/dhcps.c
    ${COREFILES}
    ${CORE4FILES}
    ${CORE6FILES}
    ${SNMPFILES}
    ${APIFILES}
    ${NETIFFILES}
    ${ARCHFILES}
    ${SNTPFILES})

if(PORT STREQUAL "unix")
set(IFSRC netif/tapif.c)
endif()
		
if(PORT STREQUAL "lpc2xxx")
set(IFSRC netif/cs8900a_lpc_e2129.c)
endif()


add_peer_directory(${PICOOS_DIR})
add_peer_directory(../picoos-micro)

add_library(picoos-lwip STATIC ${SRC})

target_include_directories(picoos-lwip
  PUBLIC include
         ${LWIPDIR}/include
         ${LWIPDIR}/include/ipv4
         ${LWIPDIR}/include/ipv6
         ${LWIPDRIVER_INC})

target_link_libraries(picoos-lwip picoos-micro picoos)
