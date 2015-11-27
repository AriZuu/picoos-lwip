/*
 * Copyright (c) 2014, Ari Suutari <ari@stonepile.fi>.
 * Copyright (c) 2005, Dennis Kuschel.
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

#include <picoos.h>

#include "lwip/debug.h"
#include "lwip/sys.h"
#include "lwip/opt.h"
#include "lwip/stats.h"

#define DEFAULT_MBOX_SIZE 10

/*
 * Mutex implementation uses Pico]OS nano layer mutex api directly.
 */

err_t sys_mutex_new(sys_mutex_t *mutex)
{
  *mutex = nosMutexCreate(0, NULL);
  if (*mutex == NULL)
    return ERR_MEM;
  
  return ERR_OK;
}

void sys_mutex_lock(sys_mutex_t *mutex)
{
  nosMutexLock(*mutex);
}

void sys_mutex_unlock(sys_mutex_t *mutex)
{
  nosMutexUnlock(*mutex);
}

void sys_mutex_free(sys_mutex_t *mutex)
{
  nosMutexDestroy(*mutex);
}

/*
 * Semaphore implementation uses Pico]OS nano layer semphore api directly.
 */

err_t sys_sem_new(sys_sem_t *sem, u8_t count)
{
  *sem = nosSemaCreate(count, 0, NULL);
  if (*sem == NULL)
    return ERR_MEM;
  
  return ERR_OK;
}

void sys_sem_signal(sys_sem_t *sem)
{
  nosSemaSignal(*sem);
}

u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout)
{
  JIF_t start = jiffies;
  u32_t w;

  if (nosSemaWait(*sem, (timeout == 0) ? INFINITE : (UINT_t) MS(timeout)) > 0)
    return SYS_ARCH_TIMEOUT;

  /* Calculate the waited time. We make sure that the calculated 
     time is never zero. Otherwise, when there is high traffic on
     this semaphore, this function may always return with zero
     and a sys.c-timer may never expire. */

  w = (u32_t) (jiffies - start) * 1000;
  w = w / HZ;
  if (w < (1000 / HZ))
    w = (HZ < 500) ? (500 / HZ) : 1;

  return (w < timeout) ? w : timeout;
}

void sys_sem_free(sys_sem_t *sem)
{
  posSemaDestroy(*sem);
}

/*
 * Mailboxes use picoos-micro UosRing buffers.
 */

err_t sys_mbox_new(sys_mbox_t *mb, int size)
{
  if (size == 0)
    size = DEFAULT_MBOX_SIZE;

  *mb = uosRingCreate(sizeof(void*), size);
  if (*mb == NULL)
    return ERR_MEM;
    
  SYS_STATS_INC_USED(mbox);
  return ERR_OK;
}

void sys_mbox_post(sys_mbox_t *mb, void *msg)
{
  LWIP_ASSERT("invalid mbox", (mb != NULL) && (*mb != NULL));

  uosRingPut(*mb, &msg, INFINITE);
}

err_t sys_mbox_trypost(sys_mbox_t *mb, void *msg)
{
  LWIP_ASSERT("invalid mbox", (mb != NULL) && (*mb != NULL));

  if (!uosRingPut(*mb, &msg, 0))
    return ERR_MEM;

  return ERR_OK;
}

u32_t sys_arch_mbox_fetch(sys_mbox_t *mb, void **msg, u32_t timeout)
{
  LWIP_ASSERT("invalid mbox", (mb != NULL) && (*mb != NULL));

  JIF_t start = jiffies;
  u32_t w;

  if (!uosRingGet(*mb, msg, (timeout == 0) ? INFINITE : (UINT_t) MS(timeout)) > 0)
    return SYS_ARCH_TIMEOUT;

  /* Calculate the waited time. We make sure that the calculated 
     time is never zero. Otherwise, when there is high traffic on
     this semaphore, this function may always return with zero
     and a sys.c-timer may never expire. */

  w = (u32_t) (jiffies - start) * 1000;
  w = w / HZ;
  if (w < (1000 / HZ))
    w = (HZ < 500) ? (500 / HZ) : 1;

  return (w < timeout) ? w : timeout;
}

u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mb, void **msg)
{
  LWIP_ASSERT("invalid mbox", (mb != NULL) && (*mb != NULL));

  if (!uosRingGet(*mb, msg, 0))
    return SYS_MBOX_EMPTY;

  return 0;
}

void sys_mbox_free(sys_mbox_t *mb)
{
  if ((mb != NULL) && (*mb != NULL)) {

    sys_mbox_t mbox = *mb;

    uosRingDestroy(mbox);
    SYS_STATS_DEC(mbox.used);
  }
}

#if SYS_LIGHTWEIGHT_PROT != 1
#error SYS_LIGHTWIGHT_PROT must be defined as 1
#endif

/*
 * Preemption protection.
 */
sys_prot_t sys_arch_protect()
{
  POS_LOCKFLAGS;

  POS_SCHED_LOCK;
  return flags;
}

void sys_arch_unprotect(sys_prot_t flags)
{
  POS_SCHED_UNLOCK;
}

/*
 * Thread creation, use Pico]OS nano layer directly.
 */
sys_thread_t sys_thread_new(const char *name, lwip_thread_fn thread, void *arg, int stacksize, int prio)
{
  return nosTaskCreate(thread, arg, prio, stacksize, name);
}

/*
 * Platform initialization.
 */
void sys_init(void)
{
}

/*
 * Random numbers.
 */
static unsigned int seedValue;
void sys_random_init(unsigned short seed)
{
  seedValue = seed;
}

int sys_random(void)
{
  return (unsigned short)rand_r(&seedValue);
}

