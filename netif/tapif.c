/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
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
 */

#include <picoos.h>
#include <stdbool.h>

#include "lwip/opt.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/stats.h"
#include "lwip/snmp.h"
#include "lwip/ethip6.h"
#include "netif/etharp.h"

#include "netif/tapif.h"

#include <sys/time.h>
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <memory.h>
#include <signal.h>
#include <errno.h>

/*
 * Interface-specific data.
 */
struct tapIf
{
  int       tap;
  NOSTASK_t poll;
  NOSSEMA_t sema;

};

static bool tapIfInput(struct netif *netif);

static void ioReadyContext(void);
static void ioReady(int sig, siginfo_t *info, void *ucontext);

static ucontext_t sigContext;
static NOSSEMA_t  pollSema;

#if PORTCFG_IRQ_STACK_SIZE >= PORTCFG_MIN_STACK_SIZE
static char sigStack[PORTCFG_IRQ_STACK_SIZE];
#else
static char sigStack[PORTCFG_MIN_STACK_SIZE];
#endif

/*
 * Handle "interrupt" from tap device when packet comes in.
 */
static void ioReadyContext()
{
  c_pos_intEnter();
  nosSemaSignal(pollSema);
  c_pos_intExit();
  setcontext(&posCurrentTask_g->ucontext);
  assert(0);
}

static void ioReady(int sig, siginfo_t *info, void *ucontext)
{
  getcontext(&sigContext);
  sigContext.uc_stack.ss_sp = sigStack;
  sigContext.uc_stack.ss_size = sizeof(sigStack);
  sigContext.uc_stack.ss_flags = 0;
  sigContext.uc_link = 0;
  sigfillset(&sigContext.uc_sigmask);

  makecontext(&sigContext, ioReadyContext, 0);
  swapcontext(&posCurrentTask_g->ucontext, &sigContext);
}

/*
 * Initialize chip.
 */
static void lowLevelInit(struct netif *netif)
{
  struct tapIf *tapIf = netif->state;

  // Set ethernet address
  netif->hwaddr_len = ETHARP_HWADDR_LEN;
  netif->hwaddr[0] = 0x0;
  netif->hwaddr[1] = 0xbd;
  netif->hwaddr[2] = 0x3;
  netif->hwaddr[3] = 0x4;
  netif->hwaddr[4] = 0x5;
  netif->hwaddr[5] = 0x6;

  netif->mtu = 1500;
  netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

  struct sigaction  sig;
  int               flags;
  char              ifconfig[80];

  tapIf->tap = open("/dev/tap0", O_RDWR);
  P_ASSERT("tap0", tapIf->tap != -1);

  sprintf (ifconfig,
           "ifconfig tap0 %d.%d.%d.%d ",
           ip4_addr1(&(netif->gw)),
           ip4_addr2(&(netif->gw)),
           ip4_addr3(&(netif->gw)),
           ip4_addr4(&(netif->gw)));

  sprintf (ifconfig + strlen(ifconfig),
           "netmask %d.%d.%d.%d up",
           ip4_addr1(&(netif->netmask)),
           ip4_addr2(&(netif->netmask)),
           ip4_addr3(&(netif->netmask)),
           ip4_addr4(&(netif->netmask)));

  system(ifconfig);

#if LWIP_IPV6
  sprintf (ifconfig, "ifconfig tap0 inet6 -ifdisabled");
  system(ifconfig);
#endif

  memset(&sig, '\0', sizeof(sig));

  sig.sa_sigaction = ioReady;
  sig.sa_flags     = SA_RESTART | SA_SIGINFO;

  sigaction(SIGIO, &sig, NULL);

  fcntl(tapIf->tap, F_SETOWN, getpid());
  flags = fcntl(tapIf->tap, F_GETFL, 0);
  fcntl(tapIf->tap, F_SETFL, flags | O_ASYNC | O_NONBLOCK);
}

/*
 * Send packet.
 */
static err_t lowLevelOutput(struct netif *netif, struct pbuf *p)
{
  struct tapIf *tapIf = netif->state;
  struct pbuf *q;
  char        ethBuf[1514];
  char        *bufPtr;

#if ETH_PAD_SIZE
  pbuf_header(p, -ETH_PAD_SIZE); /* drop the padding word */
#endif

  bufPtr = ethBuf;
  for(q = p; q != NULL; q = q->next) {

    memcpy(bufPtr, q->payload, q->len);
    bufPtr += q->len;
  }

  if (write(tapIf->tap, ethBuf, p->tot_len) == -1)
    return ERR_IF;

#if ETH_PAD_SIZE
  pbuf_header(p, ETH_PAD_SIZE); /* reclaim the padding word */
#endif

  LINK_STATS_INC(link.xmit);

  return ERR_OK;
}

/*
 * Receive packet.
 */
static struct pbuf *lowLevelInput(struct netif *netif)
{
  struct tapIf  *tapIf = netif->state;
  struct pbuf   *p, *q;
  int           len;
  char          ethBuf[1514];
  char          *bufPtr;

  // Read the packet.
  len = read(tapIf->tap, ethBuf, sizeof(ethBuf));
  if (len == -1 && errno == EAGAIN)
    return NULL;

#if ETH_PAD_SIZE
  len += ETH_PAD_SIZE; /* allow room for Ethernet padding */
#endif

  p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
  if (p != NULL) {

#if ETH_PAD_SIZE
    pbuf_header(p, -ETH_PAD_SIZE); /* drop the padding word */
#endif

    bufPtr = ethBuf;
    for(q = p; q != NULL; q = q->next) {

      memcpy(q->payload, bufPtr, q->len);
      bufPtr += q->len;
    }

#if ETH_PAD_SIZE
    pbuf_header(p, ETH_PAD_SIZE); /* reclaim the padding word */
#endif

    LINK_STATS_INC(link.recv);
  }
  else {

    LINK_STATS_INC(link.memerr);
    LINK_STATS_INC(link.drop);
  }

  return p;
}

/*
 * Receive packet and pass it to lwip stack.
 */
static bool tapIfInput(struct netif *netif)
{
  struct tapIf *tapIf;
  struct eth_hdr *ethhdr;
  struct pbuf *p;

  tapIf = netif->state;

  p = lowLevelInput(netif);
  if (p == NULL)
    return false;

  ethhdr = p->payload;

  switch (htons(ethhdr->type))
  {
  /* IP or ARP packet? */
  case ETHTYPE_IP:
  case ETHTYPE_IPV6:
  case ETHTYPE_ARP:
#if PPPOE_SUPPORT
    /* PPPoE packet? */
    case ETHTYPE_PPPOEDISC:
    case ETHTYPE_PPPOE:
#endif /* PPPOE_SUPPORT */
    /* full packet send to tcpip_thread to process */
    if (netif->input(p, netif) != ERR_OK) {
      LWIP_DEBUGF(NETIF_DEBUG, ("tapIf_input: IP input error\n"));
      pbuf_free(p);
      p = NULL;
    }
    break;

  default:
    pbuf_free(p);
    p = NULL;
    break;
  }

  return true;
}

/*
 * Polling thread
 */
static void tapThread(void* arg)
{
  struct netif* netif = (struct netif*) arg;
  struct tapIf *tapIf;

  tapIf = netif->state;
  nosPrintf("tap start.\n");

  while (true) {

    nosSemaWait(tapIf->sema, MS(1000));
    while (tapIfInput(netif))
      ;
  }
}

/*
 * Initialize interface.
 */
err_t tapIfInit(struct netif *netif)
{
  struct tapIf *tapIf;

  LWIP_ASSERT("netif != NULL", (netif != NULL));

  tapIf = mem_malloc(sizeof(struct tapIf));
  if (tapIf == NULL) {

    LWIP_DEBUGF(NETIF_DEBUG, ("tapIfInit: out of memory\n"));
    return ERR_MEM;
  }

#if LWIP_NETIF_HOSTNAME
  netif->hostname = "lwip";
#endif

  NETIF_INIT_SNMP(netif, snmp_ifType_ethernet_csmacd, 10000000);

  netif->state = tapIf;
  netif->name[0] = 't';
  netif->name[1] = 'a';
  netif->output = etharp_output;

#if LWIP_IPV6
  netif->output_ip6 = ethip6_output;
#endif

  netif->linkoutput = lowLevelOutput;

  lowLevelInit(netif);

  /*
   * Create thread to poll the interface.
   */

  tapIf->sema = nosSemaCreate(1, 0, "tap");
  tapIf->poll = nosTaskCreate(tapThread, netif, 10, 300, "cs");
  pollSema = tapIf->sema;

  return ERR_OK;
}
