/*
 * Copyright (c) 2001-2003 Swedish Institute of Computer Science.
 * Copyright (c) 2005, Dennis Kuschel.
 * Copyright (c) 2014, Ari Suutari <ari@stonepile.fi>.
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

/*
 * Use the same mailbox implementation from lwip unix port.
 */
#define SYS_MBOX_SIZE 128

struct sys_mbox {
  int         first;
  int         last;
  void        *msgs[SYS_MBOX_SIZE];
  sys_sem_t   not_empty;
  sys_sem_t   not_full;
  NOSMUTEX_t  mutex;
  int         wait_send;
};

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
 * Mailboxes, taken from lwip unix port.
 */

err_t sys_mbox_new(sys_mbox_t *mb, int size)
{
  sys_mbox_t mbox;
  LWIP_UNUSED_ARG(size);

  mbox = (struct sys_mbox *)nosMemAlloc(sizeof(struct sys_mbox));
  if (mbox == NULL)
    return ERR_MEM;

  mbox->first     = 0;
  mbox->last      = 0;
  mbox->not_empty = nosSemaCreate(0, 0, NULL);
  mbox->not_full  = nosSemaCreate(0, 0, NULL);
  mbox->mutex     = nosMutexCreate(0, NULL);
  mbox->wait_send = 0;

  SYS_STATS_INC_USED(mbox);
  *mb = mbox;
  return ERR_OK;
}

void sys_mbox_post(sys_mbox_t *mb, void *msg)
{
  u8_t first;
  sys_mbox_t mbox;
  LWIP_ASSERT("invalid mbox", (mb != NULL) && (*mb != NULL));
  mbox = *mb;

  nosMutexLock(mbox->mutex);

  LWIP_DEBUGF(SYS_DEBUG, ("sys_mbox_post: mbox %p msg %p\n", (void *)mbox, (void *)msg));

  while ((mbox->last + 1) >= (mbox->first + SYS_MBOX_SIZE)) {

    mbox->wait_send++;
 
    nosMutexUnlock(mbox->mutex);
    nosSemaWait(mbox->not_full, INFINITE);
    nosMutexLock(mbox->mutex);

    mbox->wait_send--;
  }

  mbox->msgs[mbox->last % SYS_MBOX_SIZE] = msg;

  if (mbox->last == mbox->first)
    first = 1;
  else
    first = 0;

  mbox->last++;

  if (first)
    nosSemaSignal(mbox->not_empty);

  nosMutexUnlock(mbox->mutex);
}

err_t sys_mbox_trypost(sys_mbox_t *mb, void *msg)
{
  u8_t first;
  sys_mbox_t mbox;
  LWIP_ASSERT("invalid mbox", (mb != NULL) && (*mb != NULL));
  mbox = *mb;

  nosMutexLock(mbox->mutex);

  LWIP_DEBUGF(SYS_DEBUG, ("sys_mbox_trypost: mbox %p msg %p\n",
                          (void *)mbox, (void *)msg));

  if ((mbox->last + 1) >= (mbox->first + SYS_MBOX_SIZE)) {

    nosMutexUnlock(mbox->mutex);
    return ERR_MEM;
  }

  mbox->msgs[mbox->last % SYS_MBOX_SIZE] = msg;

  if (mbox->last == mbox->first)
    first = 1;
  else
    first = 0;

  mbox->last++;

  if (first)
    nosSemaSignal(mbox->not_empty);

  nosMutexUnlock(mbox->mutex);
  return ERR_OK;
}

u32_t sys_arch_mbox_fetch(sys_mbox_t *mb, void **msg, u32_t timeout)
{
  u32_t time_needed = 0;
  sys_mbox_t mbox;
  LWIP_ASSERT("invalid mbox", (mb != NULL) && (*mb != NULL));
  mbox = *mb;

  /* The mutex lock is quick so we don't bother with the timeout
     stuff here. */
  nosMutexLock(mbox->mutex);

  while (mbox->first == mbox->last) {

    nosMutexUnlock(mbox->mutex);

    /* We block while waiting for a mail to arrive in the mailbox. We
       must be prepared to timeout. */
    if (timeout != 0) {

      time_needed = sys_arch_sem_wait(&mbox->not_empty, timeout);

      if (time_needed == SYS_ARCH_TIMEOUT)
        return SYS_ARCH_TIMEOUT;

    }
    else
      sys_arch_sem_wait(&mbox->not_empty, 0);

    nosMutexLock(mbox->mutex);
  }

  if (msg != NULL) {

    LWIP_DEBUGF(SYS_DEBUG, ("sys_mbox_fetch: mbox %p msg %p\n", (void *)mbox, *msg));
    *msg = mbox->msgs[mbox->first % SYS_MBOX_SIZE];
  }
  else {

    LWIP_DEBUGF(SYS_DEBUG, ("sys_mbox_fetch: mbox %p, null msg\n", (void *)mbox));
  }

  mbox->first++;

  if (mbox->wait_send)
    nosSemaSignal(mbox->not_full);

  nosMutexUnlock(mbox->mutex);
  return time_needed;
}

u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mb, void **msg)
{
  sys_mbox_t mbox;
  LWIP_ASSERT("invalid mbox", (mb != NULL) && (*mb != NULL));
  mbox = *mb;

  nosMutexLock(mbox->mutex);

  if (mbox->first == mbox->last) {

    nosMutexUnlock(mbox->mutex);
    return SYS_MBOX_EMPTY;
  }

  if (msg != NULL) {

    LWIP_DEBUGF(SYS_DEBUG, ("sys_mbox_tryfetch: mbox %p msg %p\n", (void *)mbox, *msg));
    *msg = mbox->msgs[mbox->first % SYS_MBOX_SIZE];
  }
  else {

    LWIP_DEBUGF(SYS_DEBUG, ("sys_mbox_tryfetch: mbox %p, null msg\n", (void *)mbox));
  }

  mbox->first++;

  if (mbox->wait_send)
    sys_sem_signal(&mbox->not_full);

  nosMutexUnlock(mbox->mutex);
  return 0;
}

void sys_mbox_free(sys_mbox_t *mb)
{
  if ((mb != NULL) && (*mb != NULL)) {

    sys_mbox_t mbox = *mb;
    SYS_STATS_DEC(mbox.used);
    nosMutexLock(mbox->mutex);
    
    nosSemaDestroy(mbox->not_empty);
    nosSemaDestroy(mbox->not_full);
    nosMutexDestroy(mbox->mutex);

    mbox->not_empty = NULL;
    mbox->not_full  = NULL;
    mbox->mutex     = NULL;

    nosMemFree(mbox);
  }
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

