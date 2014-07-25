/*
 * lwIP device driver for CS8900A chip in 8-bit mode.
 *
 * This is originally from uIP port to LPC-E2124 by Paul Curtis at Rowley Associates.
 * http://www.rowley.co.uk/msp430/uip.htm
 * 
 * The download is not available at current page, see web archive at:
 * http://web.archive.org/web/20050206190903/http://rowley.co.uk/arm/uip-e2124.zip
 *
 * Datasheet: www.cirrus.com/en/pubs/proDatasheet/CS8900A_F5.pdf
 * 8-bit application note: http://www.cirrus.com/en/pubs/appNote/an181.pdf
 */

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

#include "lpc_reg.h"
#include "netif/cs8900a_regs.h"
#include "netif/cs8900aif.h"

#define IOR                  (1<<12)  // CS8900's ISA-bus interface pins
#define IOW                  (1<<13)

#define IODIR                GPIO0_IODIR
#define IOSET                GPIO0_IOSET
#define IOCLR                GPIO0_IOCLR
#define IOPIN                GPIO0_IOPIN

// Struct for CS8900 init sequence

typedef struct
{
  unsigned int Addr;
  unsigned int Data;
} TInitSeq;

static void cs8900aSkipFrame(void);

static TInitSeq InitSeq[] =
{
    { PP_LineCTL, SERIAL_RX_ON | SERIAL_TX_ON },           // configure the Physical Interface
    { PP_LAF + 0, 0xffff },
    { PP_LAF + 2, 0xffff },
    { PP_LAF + 4, 0xffff },
    { PP_LAF + 6, 0xffff },
    { PP_RxCTL, RX_OK_ACCEPT | RX_IA_ACCEPT | RX_BROADCAST_ACCEPT | RX_MULTCAST_ACCEPT } };

/*
 * IOW must be low for 110ns min for CS8900 to get data.
 * IOR must be low for 135ns min for data to be valid.
 * According to KEIL profiling, with 60 MHz clock
 * single NOP is about 0.021 us. So seven times NOP
 * is about 147 ns, which should be ok.
 */

#define IO_DELAY()  asm volatile("nop\n\t" \
                                 "nop\n\t" \
                                 "nop\n\t" \
                                 "nop\n\t" \
                                 "nop\n\t" \
                                 "nop\n\t" \
                                 "nop");

/*
 * Interface-specific data.
 */
struct cs8900aIf
{
  NOSTASK_t poll;
};

static bool cs8900aIfInput(struct netif *netif);

// Writes a word in little-endian byte order to a specified port-address

static void cs8900aWrite(unsigned addr, unsigned int data)
{
  IODIR |= 0xff << 16;                           // Data port to output

  // Write low order byte first

  IOCLR = 0xf << 4;                              // Put address on bus
  IOSET = addr << 4;

  IOCLR = 0xff << 16;                            // Write low order byte to data bus
  IOSET = (data & 0xff) << 16;

  IOCLR = IOW;                                   // Toggle IOW-signal
  IO_DELAY();

  IOSET = IOW;

  // Write high order byte second

  IOSET = 1 << 4;                                // Put next address on bus

  IOCLR = 0xff << 16;                            // Write high order byte to data bus
  IOSET = data >> 8 << 16;

  IOCLR = IOW;                                   // Toggle IOW-signal
  IO_DELAY();

  IOSET = IOW;
}

// Reads a word in little-endian byte order from a specified port-address

static unsigned cs8900aRead(unsigned addr)
{
  unsigned int value;

  IODIR &= ~(0xff << 16);                        // Data port to input

  IOCLR = 0xf << 4;                              // Put address on bus
  IOSET = addr << 4;

  IOCLR = IOR;                                   // IOR-signal low
  IO_DELAY();

  value = (IOPIN >> 16) & 0xff;                  // get low order byte from data bus
  IOSET = IOR;

  IOSET = 1 << 4;                                // IOR high and put next address on bus

  IOCLR = IOR;                                   // IOR-signal low
  IO_DELAY();

  value |= ((IOPIN >> 8) & 0xff00);              // get high order byte from data bus
  IOSET = IOR;                                   // IOR-signal low

  return value;
}

// Reads a word in little-endian byte order from a specified port-address

static unsigned cs8900aReadAddrHighFirst(unsigned addr)
{
  unsigned int value;

  IODIR &= ~(0xff << 16);                        // Data port to input

  IOCLR = 0xf << 4;                              // Put address on bus
  IOSET = (addr + 1) << 4;

  IOCLR = IOR;                                   // IOR-signal low
  IO_DELAY();

  value = ((IOPIN >> 8) & 0xff00);               // get high order byte from data bus
  IOSET = IOR;                                   // IOR-signal high

  IOCLR = 1 << 4;                                // Put low address on bus

  IOCLR = IOR;                                   // IOR-signal low
  IO_DELAY();

  value |= (IOPIN >> 16) & 0xff;                 // get low order byte from data bus
  IOSET = IOR;

  return value;
}

static void cs8900aWriteTxBuf(uint8_t* bytes)
{
  IOCLR = 1 << 4;                                // put address on bus

  IOCLR = 0xff << 16;                            // Write low order byte to data bus
  IOSET = bytes[0] << 16;                        // write low order byte to data bus

  IOCLR = IOW;                                   // Toggle IOW-signal
  IO_DELAY();

  IOSET = IOW;

  IOSET = 1 << 4;                                // Put next address on bus

  IOCLR = 0xff << 16;                            // Write low order byte to data bus
  IOSET = bytes[1] << 16;                        // write low order byte to data bus

  IOCLR = IOW;                                   // Toggle IOW-signal
  IO_DELAY();

  IOSET = IOW;
}

static void cs8900aReadRxBuf(uint8_t* bytes)
{
  IOCLR = 1 << 4;                          // put address on bus

  IOCLR = IOR;                             // IOR-signal low
  IO_DELAY();

  bytes[0] = IOPIN >> 16;                  // get high order byte from data bus
  IOSET = IOR;                             // IOR-signal high

  IOSET = 1 << 4;                          // put address on bus

  IOCLR = IOR;                             // IOR-signal low
  IO_DELAY();

  bytes[1] = IOPIN >> 16;                  // get high order byte from data bus
  IOSET = IOR;                             // IOR-signal high
}

static void cs8900aSkipFrame(void)
{
  // No space avaliable, skip a received frame and try again
  cs8900aWrite(ADD_PORT, PP_RxCFG);
  cs8900aWrite(DATA_PORT, cs8900aRead(DATA_PORT) | SKIP_1);
}

/*
 * Initialize chip.
 */
static void lowLevelInit(struct netif *netif)
{
  // Set ethernet address
  netif->hwaddr_len = ETHARP_HWADDR_LEN;
  netif->hwaddr[0] = 0x0;
  netif->hwaddr[1] = 0xbd;
  netif->hwaddr[2] = 0x3;
  netif->hwaddr[3] = 0x4;
  netif->hwaddr[4] = 0x5;
  netif->hwaddr[5] = 0x7;

  netif->mtu = 1500;
  netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

  unsigned int i;

  // Reset outputs, control lines high
  IOSET = IOR | IOW;

  // Port 3 output pins
  // Bits 4-7: SA 0-3
  // Bit 12: IOR
  // Bit 13: IOW
  // Bits 16-23: SD 0-7
  IODIR |= (0xff << 16) | IOR | IOW | (0xf << 4);

  // Reset outputs
  IOCLR = 0xff << 16;  // clear data outputs

  // Reset the CS8900A
  cs8900aWrite(ADD_PORT, PP_SelfCTL);
  cs8900aWrite(DATA_PORT, POWER_ON_RESET);

  // Wait until chip-reset is done
  cs8900aWrite(ADD_PORT, PP_SelfST);
  while ((cs8900aRead(DATA_PORT) & INIT_DONE) == 0)
    ;

  // Set ethernet address.
  for (i = 0; i < 6; i += 2) {

    cs8900aWrite(ADD_PORT, PP_IA + i);
    cs8900aWrite(DATA_PORT, netif->hwaddr[i] + (netif->hwaddr[i + 1] << 8));
  }

  // Configure the CS8900A
  for (i = 0; i < sizeof InitSeq / sizeof(TInitSeq); ++i) {

    cs8900aWrite(ADD_PORT, InitSeq[i].Addr);
    cs8900aWrite(DATA_PORT, InitSeq[i].Data);
  }
}

/*
 * Send packet.
 */
static err_t lowLevelOutput(struct netif *netif, struct pbuf *p)
{
  struct pbuf *q;
  unsigned u;

#if ETH_PAD_SIZE
  pbuf_header(p, -ETH_PAD_SIZE); /* drop the padding word */
#endif

  // Transmit command
  cs8900aWrite(TX_CMD_PORT, TX_START_ALL_BYTES);
  cs8900aWrite(TX_LEN_PORT, p->tot_len);

  // Maximum number of retries
  u = 8;
  for (;;) {

    // Check for avaliable buffer space
    cs8900aWrite(ADD_PORT, PP_BusST);
    if (cs8900aRead(DATA_PORT) & READY_FOR_TX_NOW)
      break;

    if (u-- == 0)
      return ERR_IF;

    // No space avaliable, skip a received frame and try again
    cs8900aSkipFrame();
  }

  for (q = p; q != NULL; q = q->next) {

    IODIR |= 0xff << 16;                             // Data port to output
    IOCLR = 0xf << 4;                                // Put address on bus
    IOSET = TX_FRAME_PORT << 4;

    // Send packet.
    for (u = 0; u < q->len; u += 2)
      cs8900aWriteTxBuf(((uint8_t*)q->payload) + u);
  }

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
  struct pbuf *p, *q;
  u16_t len;
  uint16_t u;

  // Check receiver event register to see if there are any valid frames avaliable
  cs8900aWrite(ADD_PORT, PP_RxEvent);
  if ((cs8900aRead(DATA_PORT) & 0xd00) == 0)
    return NULL;

  // Read receiver status and discard it.
  cs8900aReadAddrHighFirst(RX_FRAME_PORT);

  // Read frame length
  len = cs8900aReadAddrHighFirst(RX_FRAME_PORT);

#if ETH_PAD_SIZE
  len += ETH_PAD_SIZE; /* allow room for Ethernet padding */
#endif

  p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
  if (p != NULL) {

#if ETH_PAD_SIZE
    pbuf_header(p, -ETH_PAD_SIZE); /* drop the padding word */
#endif

    // Data port to input
    IODIR &= ~(0xff << 16);

    IOCLR = 0xf << 4;                          // put address on bus
    IOSET = RX_FRAME_PORT << 4;

    for (q = p; q != NULL; q = q->next) {

      u = 0;
      while (u < q->len) {

        cs8900aReadRxBuf(((uint8_t*)q->payload) + u);
        u += 2;
      }
    }

#if ETH_PAD_SIZE
    pbuf_header(p, ETH_PAD_SIZE); /* reclaim the padding word */
#endif

    LINK_STATS_INC(link.recv);
  }
  else {

    cs8900aSkipFrame();
    LINK_STATS_INC(link.memerr);
    LINK_STATS_INC(link.drop);
  }

  return p;
}

/*
 * Receive packet and pass it to lwip stack.
 */
static bool cs8900aIfInput(struct netif *netif)
{
  struct cs8900aIf *cs8900aIf;
  struct eth_hdr *ethhdr;
  struct pbuf *p;

  cs8900aIf = netif->state;

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
      LWIP_DEBUGF(NETIF_DEBUG, ("cs8900aIf_input: IP input error\n"));
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
static void cs8900aThread(void* arg)
{
  struct netif* netif = (struct netif*) arg;

  nosPrintf("cs8900a start.\n");

  while (true) {

    posTaskSleep(MS(10));
    while (cs8900aIfInput(netif))
      ;
  }
}

/*
 * Initialize interface.
 */
err_t cs8900aIfInit(struct netif *netif)
{
  struct cs8900aIf *cs8900aIf;

  LWIP_ASSERT("netif != NULL", (netif != NULL));

  cs8900aIf = mem_malloc(sizeof(struct cs8900aIf));
  if (cs8900aIf == NULL) {

    LWIP_DEBUGF(NETIF_DEBUG, ("cs8900aIfInit: out of memory\n"));
    return ERR_MEM;
  }

#if LWIP_NETIF_HOSTNAME
  netif->hostname = "lwip";
#endif

  NETIF_INIT_SNMP(netif, snmp_ifType_ethernet_csmacd, 10000000);

  netif->state = cs8900aIf;
  netif->name[0] = 'c';
  netif->name[1] = 's';
  netif->output = etharp_output;

#if LWIP_IPV6
  netif->output_ip6 = ethip6_output;
#endif

  netif->linkoutput = lowLevelOutput;

  lowLevelInit(netif);

  /*
   * Create thread to poll the interface.
   */

  cs8900aIf->poll = nosTaskCreate(cs8900aThread, netif, 1, 300, "cs");

  return ERR_OK;
}

