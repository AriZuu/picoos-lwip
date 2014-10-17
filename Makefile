#
# Copyright (c) 2012-2014, Ari Suutari <ari@stonepile.fi>.
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

#
# Compile uIP & drivers using Pico]OS library Makefile
#

RELROOT = ../picoos/
PORT ?= unix
BUILD ?= RELEASE

include $(RELROOT)make/common.mak

TARGET = picoos-lwip

#
# lwip source files
#
LWIPDIR=lwip/src
# COREFILES, CORE4FILES: The minimum set of files needed for lwIP.
COREFILES =	$(LWIPDIR)/core/def.c $(LWIPDIR)/core/dhcp.c $(LWIPDIR)/core/dns.c \
		$(LWIPDIR)/core/inet_chksum.c $(LWIPDIR)/core/init.c $(LWIPDIR)/core/mem.c \
		$(LWIPDIR)/core/memp.c $(LWIPDIR)/core/netif.c $(LWIPDIR)/core/pbuf.c \
		$(LWIPDIR)/core/raw.c $(LWIPDIR)/core/stats.c $(LWIPDIR)/core/sys.c \
		$(LWIPDIR)/core/tcp.c $(LWIPDIR)/core/tcp_in.c \
		$(LWIPDIR)/core/tcp_out.c $(LWIPDIR)/core/timers.c $(LWIPDIR)/core/udp.c

CORE4FILES =	$(LWIPDIR)/core/ipv4/autoip.c $(LWIPDIR)/core/ipv4/icmp.c \
		$(LWIPDIR)/core/ipv4/igmp.c $(LWIPDIR)/core/ipv4/ip_frag.c \
		$(LWIPDIR)/core/ipv4/ip4.c $(LWIPDIR)/core/ipv4/ip4_addr.c

CORE6FILES =	$(LWIPDIR)/core/ipv6/dhcp6.c $(LWIPDIR)/core/ipv6/ethip6.c \
		$(LWIPDIR)/core/ipv6/icmp6.c $(LWIPDIR)/core/ipv6/ip6.c \
		$(LWIPDIR)/core/ipv6/ip6_addr.c $(LWIPDIR)/core/ipv6/ip6_frag.c \
		$(LWIPDIR)/core/ipv6/mld6.c $(LWIPDIR)/core/ipv6/nd6.c

# SNMPFILES: Extra SNMPv1 agent
SNMPFILES =	$(LWIPDIR)/core/snmp/asn1_dec.c $(LWIPDIR)/core/snmp/asn1_enc.c \
		$(LWIPDIR)/core/snmp/mib2.c $(LWIPDIR)/core/snmp/mib_structs.c \
		$(LWIPDIR)/core/snmp/msg_in.c $(LWIPDIR)/core/snmp/msg_out.c

# APIFILES: The files which implement the sequential and socket APIs.
APIFILES =	$(LWIPDIR)/api/api_lib.c $(LWIPDIR)/api/api_msg.c $(LWIPDIR)/api/err.c \
		$(LWIPDIR)/api/netbuf.c $(LWIPDIR)/api/netdb.c $(LWIPDIR)/api/netifapi.c \
		$(LWIPDIR)/api/sockets.c $(LWIPDIR)/api/tcpip.c

# NETIFFILES: Files implementing various generic network interface functions.'
NETIFFILES =	$(LWIPDIR)/netif/etharp.c

# ARCHFILES: Architecture specific files.
ARCHFILES =	sys_arch.c

SRC_TXT =	$(COREFILES) $(CORE4FILES) $(CORE6FILES) $(SNMPFILES) $(APIFILES) $(NETIFFILES) $(ARCHFILES)
EXTRA_CFLAGS = -Wno-address
SRC_HDR =	
SRC_OBJ =

ifeq '$(PORT)' 'unix'
SRC_TXT +=  netif/tapif.c
endif
		
ifeq '$(PORT)' 'lpc2xxx'
SRC_TXT +=  netif/cs8900a_lpc_e2129.c
endif

MODULES =	$(RELROOT)/../picoos-micro 
DIR_USRINC +=	include \
		$(LWIPDIR)/include \
		$(LWIPDIR)/include/ipv4 \
		$(LWIPDIR)/include/ipv6

ifeq '$(strip $(DIR_OUTPUT))' ''
DIR_OUTPUT = $(CURRENTDIR)/bin
endif


include $(MAKE_LIB)
