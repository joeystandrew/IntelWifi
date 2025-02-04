/******************************************************************************
 *
 * Copyright(c) 2003 - 2014 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 - 2015 Intel Mobile Communications GmbH
 * Copyright(c) 2016 - 2017 Intel Deutschland GmbH
 *
 * Portions of this file are derived from the ipw3945 project, as well
 * as portions of the ieee80211 subsystem header files.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 * The full GNU General Public License is included in this distribution in the
 * file called LICENSE.
 *
 * Contact Information:
 *  Intel Linux Wireless <linuxwifi@intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 *****************************************************************************/

//
//  IntelWifi_rx.cpp
//  IntelWifi
//
//  Created by Roman Peshkov on 02/01/2018.
//  Copyright © 2018 Roman Peshkov. All rights reserved.
//

#include "IntelWifi.hpp"

#include <IOKit/IODMACommand.h>
#include <sys/queue.h>

extern "C" {
#include "iwl-fh.h"
#include "iwl-trans.h"
#include "iwlwifi/fw/api/commands.h"
#include "iw_utils/allocation.h"
}

/******************************************************************************
 *
 * RX path functions
 *
 ******************************************************************************/

/*
 * Rx theory of operation
 *
 * Driver allocates a circular buffer of Receive Buffer Descriptors (RBDs),
 * each of which point to Receive Buffers to be filled by the NIC.  These get
 * used not only for Rx frames, but for any command response or notification
 * from the NIC.  The driver and NIC manage the Rx buffers by means
 * of indexes into the circular buffer.
 *
 * Rx Queue Indexes
 * The host/firmware share two index registers for managing the Rx buffers.
 *
 * The READ index maps to the first position that the firmware may be writing
 * to -- the driver can read up to (but not including) this position and get
 * good data.
 * The READ index is managed by the firmware once the card is enabled.
 *
 * The WRITE index maps to the last position the driver has read from -- the
 * position preceding WRITE is the last slot the firmware can place a packet.
 *
 * The queue is empty (no good data) if WRITE = READ - 1, and is full if
 * WRITE = READ.
 *
 * During initialization, the host sets up the READ queue position to the first
 * INDEX position, and WRITE to the last (READ - 1 wrapped)
 *
 * When the firmware places a packet in a buffer, it will advance the READ index
 * and fire the RX interrupt.  The driver can then query the READ index and
 * process as many packets as possible, moving the WRITE index forward as it
 * resets the Rx queue buffers with new memory.
 *
 * The management in the driver is as follows:
 * + A list of pre-allocated RBDs is stored in iwl->rxq->rx_free.
 *   When the interrupt handler is called, the request is processed.
 *   The page is either stolen - transferred to the upper layer
 *   or reused - added immediately to the iwl->rxq->rx_free list.
 * + When the page is stolen - the driver updates the matching queue's used
 *   count, detaches the RBD and transfers it to the queue used list.
 *   When there are two used RBDs - they are transferred to the allocator empty
 *   list. Work is then scheduled for the allocator to start allocating
 *   eight buffers.
 *   When there are another 6 used RBDs - they are transferred to the allocator
 *   empty list and the driver tries to claim the pre-allocated buffers and
 *   add them to iwl->rxq->rx_free. If it fails - it continues to claim them
 *   until ready.
 *   When there are 8+ buffers in the free list - either from allocation or from
 *   8 reused unstolen pages - restock is called to update the FW and indexes.
 * + In order to make sure the allocator always has RBDs to use for allocation
 *   the allocator has initial pool in the size of num_queues*(8-2) - the
 *   maximum missing RBDs per allocation request (request posted with 2
 *    empty RBDs, there is no guarantee when the other 6 RBDs are supplied).
 *   The queues supplies the recycle of the rest of the RBDs.
 * + A received packet is processed and handed to the kernel network stack,
 *   detached from the iwl->rxq.  The driver 'processed' index is updated.
 * + If there are no allocated buffers in iwl->rxq->rx_free,
 *   the READ INDEX is not incremented and iwl->status(RX_STALLED) is set.
 *   If there were enough free buffers and RX_STALLED is set it is cleared.
 *
 *
 * Driver sequence:
 *
 * iwl_rxq_alloc()            Allocates rx_free
 * iwl_pcie_rx_replenish()    Replenishes rx_free list from rx_used, and calls
 *                            iwl_pcie_rxq_restock.
 *                            Used only during initialization.
 * iwl_pcie_rxq_restock()     Moves available buffers from rx_free into Rx
 *                            queue, updates firmware pointers, and updates
 *                            the WRITE index.
 * iwl_pcie_rx_allocator()     Background work for allocating pages.
 *
 * -- enable interrupts --
 * ISR - iwl_rx()             Detach iwl_rx_mem_buffers from pool up to the
 *                            READ INDEX, detaching the SKB from the pool.
 *                            Moves the packet buffer from queue to rx_used.
 *                            Posts and claims requests to the allocator.
 *                            Calls iwl_pcie_rxq_restock to refill any empty
 *                            slots.
 *
 * RBD life-cycle:
 *
 * Init:
 * rxq.pool -> rxq.rx_used -> rxq.rx_free -> rxq.queue
 *
 * Regular Receive interrupt:
 * Page Stolen:
 * rxq.queue -> rxq.rx_used -> allocator.rbd_empty ->
 * allocator.rbd_allocated -> rxq.rx_free -> rxq.queue
 * Page not Stolen:
 * rxq.queue -> rxq.rx_free -> rxq.queue
 * ...
 *
 */

/*
 * CUSTOM
 */

static dma_addr_t iwl_dmamap_mbuf(struct iwl_trans *trans, mbuf_t m) {
    IOMbufNaturalMemoryCursor *curs = static_cast<IOMbufNaturalMemoryCursor *>(trans->mbuf_cursor);
    IOPhysicalSegment rxSeg;
    curs->getPhysicalSegments(m, &rxSeg, 1);
    return rxSeg.location;
}

static void iwl_free_packet(struct iwl_trans *trans, mbuf_t p) {
    IO80211Controller *dev = static_cast<IO80211Controller *>(trans->dev);
//    dev->freePacket(p);
    return;
}

/*
 * CUSTOM END
 */

/* line 139
 * iwl_rxq_space - Return number of free slots available in queue.
 */
static int iwl_rxq_space(const struct iwl_rxq *rxq)
{
    /* Make sure rx queue size is a power of 2 */
    if (rxq->queue_size & (rxq->queue_size - 1)) {
        IWL_WARN(rxq, "rxq->queue_size must be a power of 2");
    }
    
    /*
     * There can be up to (RX_QUEUE_ISZE - 1) free slots, to avoid ambiguity
     * between empty and completely full queues.
     * The following is equivalent to modulo by RX_QUEUE_SIZE and is well
     * defined for negative dividends.
     */
    return (rxq->read - rxq->write - 1) & (rxq->queue_size - 1);
}

/* line 156
 * iwl_dma_addr2rbd_ptr - convert a DMA address to a uCode read buffer ptr
 */
static inline __le32 iwl_pcie_dma_addr2rbd_ptr(dma_addr_t dma_addr)
{
    return cpu_to_le32((u32)(dma_addr >> 8));
}

/* line 164
 * iwl_pcie_rx_stop - stops the Rx DMA
 */
int iwl_pcie_rx_stop(struct iwl_trans *trans)
{
    if (trans->cfg->mq_rx_supported) {
        iwl_write_prph(trans, RFH_RXF_DMA_CFG, 0);
        return iwl_poll_prph_bit(trans, RFH_GEN_STATUS, RXF_DMA_IDLE, RXF_DMA_IDLE, 1000);
    } else {
        iwl_write_direct32(trans, FH_MEM_RCSR_CHNL0_CONFIG_REG, 0);
        return iwl_poll_direct_bit(trans, FH_MEM_RSSR_RX_STATUS_REG, FH_RSSR_CHNL0_RX_STATUS_CHNL_IDLE, 1000);
    }
}

/* line 181
 * iwl_pcie_rxq_inc_wr_ptr - Update the write pointer for the RX queue
 */
static void iwl_pcie_rxq_inc_wr_ptr(struct iwl_trans *trans, struct iwl_rxq *rxq)
{
    u32 reg;
    
    /*
     * explicitly wake up the NIC if:
     * 1. shadow registers aren't enabled
     * 2. there is a chance that the NIC is asleep
     */
    if (!trans->cfg->base_params->shadow_reg_enable &&
        test_bit(STATUS_TPOWER_PMI, &trans->status)) {
        reg = iwl_read32(trans, CSR_UCODE_DRV_GP1);
        
        if (reg & CSR_UCODE_DRV_GP1_BIT_MAC_SLEEP) {
            IWL_DEBUG_INFO(trans, "Rx queue requesting wakeup, GP1 = 0x%x\n", reg);
            iwl_set_bit(trans, CSR_GP_CNTRL, CSR_GP_CNTRL_REG_FLAG_MAC_ACCESS_REQ);
            rxq->need_update = true;
            return;
        }
    }
    
    rxq->write_actual = round_down(rxq->write, 8);
    
    if (trans->cfg->mq_rx_supported)
        iwl_write32(trans, RFH_Q_FRBDCB_WIDX_TRG(rxq->id), rxq->write_actual);
    else
        iwl_write32(trans, FH_RSCSR_CHNL0_WPTR, rxq->write_actual);
}

// line 218
static void iwl_pcie_rxq_check_wrptr(struct iwl_trans *trans)
{
    struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
    int i;
    
    for (i = 0; i < trans->num_rx_queues; i++) {
        struct iwl_rxq *rxq = &trans_pcie->rxq[i];
        
        if (!rxq->need_update)
            continue;
        //IOSimpleLockLock(rxq->lock);
        iwl_pcie_rxq_inc_wr_ptr(trans, rxq);
        rxq->need_update = false;
        //IOSimpleLockUnlock(rxq->lock);
    }
}

/* line 235
 * iwl_pcie_rxmq_restock - restock implementation for multi-queue rx
 */
static void iwl_pcie_rxmq_restock(struct iwl_trans *trans, struct iwl_rxq *rxq)
{
    struct iwl_rx_mem_buffer *rxb;
    
    /*
     * If the device isn't enabled - no need to try to add buffers...
     * This can happen when we stop the device and still have an interrupt
     * pending. We stop the APM before we sync the interrupts because we
     * have to (see comment there). On the other hand, since the APM is
     * stopped, we cannot access the HW (in particular not prph).
     * So don't try to restock if the APM has been already stopped.
     */
    if (!test_bit(STATUS_DEVICE_ENABLED, &trans->status))
        return;
    
    //IOSimpleLockLock(rxq->lock);
    while (rxq->free_count) {
        __le64 *bd = (__le64 *)rxq->bd;
        
        /* Get next free Rx buffer, remove from free list */
        rxb = TAILQ_FIRST(&rxq->rx_free);
        TAILQ_REMOVE(&rxq->rx_free, rxb, list);
        rxb->invalid = false;
        /* 12 first bits are expected to be empty */
        
        if (rxb->page_dma & DMA_BIT_MASK(12)) {
            DebugLog("12 first bits are expected to be empty");
        }
        /* Point to Rx buffer via next RBD in circular buffer */
        bd[rxq->write] = cpu_to_le64(rxb->page_dma | rxb->vid);
        rxq->write = (rxq->write + 1) & MQ_RX_TABLE_MASK;
        rxq->free_count--;
    }
    //IOSimpleLockUnlock(rxq->lock);
    
    /*
     * If we've added more space for the firmware to place data, tell it.
     * Increment device's write pointer in multiples of 8.
     */
    if (rxq->write_actual != (rxq->write & ~0x7)) {
        //IOSimpleLockLock(rxq->lock);
        iwl_pcie_rxq_inc_wr_ptr(trans, rxq);
        //IOSimpleLockUnlock(rxq->lock);
    }
}

/* line 283
 * iwl_pcie_rxsq_restock - restock implementation for single queue rx
 */
static void iwl_pcie_rxsq_restock(struct iwl_trans *trans, struct iwl_rxq *rxq)
{
    struct iwl_rx_mem_buffer *rxb;

    /*
     * If the device isn't enabled - not need to try to add buffers...
     * This can happen when we stop the device and still have an interrupt
     * pending. We stop the APM before we sync the interrupts because we
     * have to (see comment there). On the other hand, since the APM is
     * stopped, we cannot access the HW (in particular not prph).
     * So don't try to restock if the APM has been already stopped.
     */
    if (!test_bit(STATUS_DEVICE_ENABLED, &trans->status)) {
        return;
    }
    
    //IOSimpleLockLock(rxq->lock);
    while ((iwl_rxq_space(rxq) > 0) && (rxq->free_count)) {
        __le32 *bd = (__le32 *)rxq->bd;
        /* The overwritten rxb must be a used one */
        rxb = rxq->queue[rxq->write];
        //BUG_ON(rxb && rxb->page);
        
        /* Get next free Rx buffer, remove from free list */
        rxb = TAILQ_FIRST(&rxq->rx_free);
        TAILQ_REMOVE(&rxq->rx_free, rxb, list);
        rxb->invalid = false;
        
        /* Point to Rx buffer via next RBD in circular buffer */
        bd[rxq->write] = iwl_pcie_dma_addr2rbd_ptr(rxb->page_dma);
        rxq->queue[rxq->write] = rxb;
        rxq->write = (rxq->write + 1) & RX_QUEUE_MASK;
        rxq->free_count--;
    }
    //IOSimpleLockUnlock(rxq->lock);
    
    /* If we've added more space for the firmware to place data, tell it.
     * Increment device's write pointer in multiples of 8. */
    if (rxq->write_actual != (rxq->write & ~0x7)) {
        //IOSimpleLockLock(rxq->lock);
        iwl_pcie_rxq_inc_wr_ptr(trans, rxq);
        //IOSimpleLockUnlock(rxq->lock);
    }
}


/* line 332
 * iwl_pcie_rxq_restock - refill RX queue from pre-allocated pool
 *
 * If there are slots in the RX queue that need to be restocked,
 * and we have free pre-allocated buffers, fill the ranks as much
 * as we can, pulling from rx_free.
 *
 * This moves the 'write' index forward to catch up with 'processed', and
 * also updates the memory address in the firmware to reference the new
 * target buffer.
 */
static void iwl_pcie_rxq_restock(struct iwl_trans *trans, struct iwl_rxq *rxq)
{
    if (trans->cfg->mq_rx_supported)
        iwl_pcie_rxmq_restock(trans, rxq);
    else
        iwl_pcie_rxsq_restock(trans, rxq);
}

/* line 352
 * iwl_pcie_rx_alloc_page - allocates and returns a page.
 *
 */
static mbuf_t iwl_pcie_rx_alloc_page(struct iwl_trans *trans)
{
    IO80211Controller *dev = static_cast<IO80211Controller *>(trans->dev);
    mbuf_t m = dev->allocatePacket(PAGE_SIZE);
    return m;
}

/* line 384
 * iwl_pcie_rxq_alloc_rbs - allocate a page for each used RBD
 *
 * A used RBD is an Rx buffer that has been given to the stack. To use it again
 * a page must be allocated and the RBD must point to the page. This function
 * doesn't change the HW pointer but handles the list of pages that is used by
 * iwl_pcie_rxq_restock. The latter function will update the HW to use the newly
 * allocated buffers.
 */
static void iwl_pcie_rxq_alloc_rbs(struct iwl_trans *trans, struct iwl_rxq *rxq)
{
    //struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
    struct iwl_rx_mem_buffer *rxb;
    mbuf_t page;
    
    while (1) {
        //IOSimpleLockLock(rxq->lock);
        if (TAILQ_EMPTY(&rxq->rx_used)) {
            //IOSimpleLockUnlock(rxq->lock);
            return;
        }
        //IOSimpleLockUnlock(rxq->lock);
        
        /* Alloc a new receive buffer */
        page = iwl_pcie_rx_alloc_page(trans);
        if (!page)
            return;
        
        //IOSimpleLockLock(rxq->lock);
        
        if (TAILQ_EMPTY(&rxq->rx_used)) {
            //IOSimpleLockUnlock(rxq->lock);
            //__free_pages(page, trans_pcie->rx_page_order);
            iwl_free_packet(trans, page);
            page = NULL;
            return;
        }

        rxb = TAILQ_FIRST(&rxq->rx_used);
        TAILQ_REMOVE(&rxq->rx_used, rxb, list);
        //IOSimpleLockUnlock(rxq->lock);
        
        rxb->page = page;
        /* Get physical address of the RB */
        rxb->page_dma = iwl_dmamap_mbuf(trans, rxb->page);
        
        if (!rxb->page_dma) {
            
            rxb->page = NULL;
            
            //IOSimpleLockLock(rxq->lock);
            TAILQ_INSERT_HEAD(&rxq->rx_used, rxb, list);
            //IOSimpleLockUnlock(rxq->lock);
            iwl_free_packet(trans, page);
            return;
        }
        
        //IOSimpleLockLock(rxq->lock);
        TAILQ_INSERT_TAIL(&rxq->rx_free, rxb, list);
        rxq->free_count++;
        //IOSimpleLockUnlock(rxq->lock);
    }
}

// line 450
static void iwl_pcie_free_rbs_pool(struct iwl_trans *trans)
{
    struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
    int i;
    
    for (i = 0; i < RX_POOL_SIZE; i++) {
        if (!trans_pcie->rx_pool[i].page)
            continue;
        trans_pcie->rx_pool[i].page_dma = NULL;
        iwl_free_packet(trans, trans_pcie->rx_pool[i].page);
        trans_pcie->rx_pool[i].page = NULL;
    }
}

/* line 467
 * iwl_pcie_rx_allocator - Allocates pages in the background for RX queues
 *
 * Allocates for each received request 8 pages
 * Called as a scheduled work item.
 */
static void iwl_pcie_rx_allocator(struct iwl_trans *trans)
{
    struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
    struct iwl_rb_allocator *rba = &trans_pcie->rba;
    TAILQ_HEAD(, iwl_rx_mem_buffer) local_empty = TAILQ_HEAD_INITIALIZER(local_empty);
    
    // initial code: int pending = atomic_xchg(&rba->req_pending, 0);
    int pending = OSAddAtomic(-rba->req_pending, &rba->req_pending);
    IWL_DEBUG_RX(trans, "Pending allocation requests = %d\n", pending);
    
    /* If we were scheduled - there is at least one request */
    //IOSimpleLockLock(rba->lock);
    /* swap out the rba->rbd_empty to a local list */
    TAILQ_SWAP(&rba->rbd_empty, &local_empty, iwl_rx_mem_buffer, list);
    TAILQ_INIT(&rba->rbd_empty);
    //IOSimpleLockUnlock(rba->lock);
    
    while (pending) {
        int i;
        TAILQ_HEAD(, iwl_rx_mem_buffer) local_allocated = TAILQ_HEAD_INITIALIZER(local_allocated);
        
        /* Do not post a warning if there are only a few requests */
//        if (pending < RX_PENDING_WATERMARK)
//            gfp_mask |= __GFP_NOWARN;
        
        for (i = 0; i < RX_CLAIM_REQ_ALLOC;) {
            struct iwl_rx_mem_buffer *rxb;
            mbuf_t page;
            
            /* List should never be empty - each reused RBD is
             * returned to the list, and initial pool covers any
             * possible gap between the time the page is allocated
             * to the time the RBD is added.
             */
            //BUG_ON(list_empty(&local_empty));
            if (TAILQ_EMPTY(&local_empty)) {
                IWL_ERR(trans, "local_empty should never be empty!");
                break;
            }
            /* Get the first rxb from the rbd list */
            rxb = TAILQ_FIRST(&local_empty);
            //BUG_ON(rxb->page);
            
            /* Alloc a new receive buffer */
            page = iwl_pcie_rx_alloc_page(trans);
            if (!page)
                continue;
            rxb->page = page;
            
            /* Get physical address of the RB */
            rxb->page_dma = iwl_dmamap_mbuf(trans, rxb->page);
            if (!rxb->page_dma) {
                iwl_free_packet(trans, page);
                rxb->page = NULL;
                continue;
            }
            
            /* move the allocated entry to the out list */
            TAILQ_REMOVE(&local_empty, rxb, list);
            TAILQ_INSERT_TAIL(&local_allocated, rxb, list);
            i++;
        }
        
        pending--;
        if (!pending) {
            // initial code: pending = atomic_xchg(&rba->req_pending, 0);
            pending = OSAddAtomic(-rba->req_pending, &rba->req_pending);
            IWL_DEBUG_RX(trans, "Pending allocation requests = %d\n", pending);
        }
        //IOSimpleLockLock(rba->lock);
        /* add the allocated rbds to the allocator allocated list */
        if (!TAILQ_EMPTY(&local_allocated))
            TAILQ_CONCAT(&rba->rbd_allocated, &local_allocated, list);
        
        /* get more empty RBDs for current pending requests */
        if (!TAILQ_EMPTY(&rba->rbd_empty))
            TAILQ_CONCAT(&local_empty, &rba->rbd_empty, list);

        //IOSimpleLockUnlock(rba->lock);
        
        OSIncrementAtomic(&rba->req_ready);
    }
    
    //IOSimpleLockLock(rba->lock);
    /* return unused rbds to the allocator empty list */
    if (!TAILQ_EMPTY(&local_empty))
        TAILQ_CONCAT(&rba->rbd_empty, &local_empty, list);
    //IOSimpleLockUnlock(rba->lock);
}

/* line 557
 * iwl_pcie_rx_allocator_get - returns the pre-allocated pages
 .*
 .* Called by queue when the queue posted allocation request and
 * has freed 8 RBDs in order to restock itself.
 * This function directly moves the allocated RBs to the queue's ownership
 * and updates the relevant counters.
 */
static void iwl_pcie_rx_allocator_get(struct iwl_trans *trans, struct iwl_rxq *rxq)
{
    struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
    struct iwl_rb_allocator *rba = &trans_pcie->rba;
    int i;
    
    /*
     * atomic_dec_if_positive returns req_ready - 1 for any scenario.
     * If req_ready is 0 atomic_dec_if_positive will return -1 and this
     * function will return early, as there are no ready requests.
     * atomic_dec_if_positive will perofrm the *actual* decrement only if
     * req_ready > 0, i.e. - there are ready requests and the function
     * hands one request to the caller.
     */
    if (OSDecrementAtomic(&rba->req_ready) < 0) {
        rba->req_ready = 0;
        return;
    }
    
    //IOSimpleLockLock(rba->lock);
    for (i = 0; i < RX_CLAIM_REQ_ALLOC; i++) {
        /* Get next free Rx buffer, remove it from free list */
        struct iwl_rx_mem_buffer *rxb = TAILQ_FIRST(&rba->rbd_allocated);
        TAILQ_REMOVE(&rba->rbd_allocated, rxb, list);
        TAILQ_INSERT_TAIL(&rxq->rx_free, rxb, list);
    }
    //IOSimpleLockUnlock(rba->lock);
    
    rxq->used_count -= RX_CLAIM_REQ_ALLOC;
    rxq->free_count += RX_CLAIM_REQ_ALLOC;
}

// line 600
//void iwl_pcie_rx_allocator_work(struct work_struct *data)
//{
//    struct iwl_rb_allocator *rba_p =
//            container_of(data, struct iwl_rb_allocator, rx_alloc);
//    struct iwl_trans_pcie *trans_pcie =
//            container_of(rba_p, struct iwl_trans_pcie, rba);
//
//    iwl_pcie_rx_allocator(trans_pcie->trans);
//}

// line 610
static int iwl_pcie_rx_alloc(struct iwl_trans *trans)
{
    struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
    struct iwl_rb_allocator *rba = &trans_pcie->rba;
    //struct device *dev = trans->dev;
    int i;
    int free_size = trans->cfg->mq_rx_supported ? sizeof(__le64) : sizeof(__le32);
    
    if (WARN_ON(trans_pcie->rxq))
        return -EINVAL;
    
    trans_pcie->rxq = (struct iwl_rxq *)iwh_zalloc(sizeof(struct iwl_rxq) * trans->num_rx_queues);
    
    if (!trans_pcie->rxq)
        return -EINVAL;
    
    rba->lock = IOSimpleLockAlloc();
    
    for (i = 0; i < trans->num_rx_queues; i++) {
        struct iwl_rxq *rxq = &trans_pcie->rxq[i];
        
        rxq->lock = IOSimpleLockAlloc();
        if (trans->cfg->mq_rx_supported)
            rxq->queue_size = MQ_RX_TABLE_SIZE;
        else
            rxq->queue_size = RX_QUEUE_SIZE;
        
        /*
         * Allocate the circular buffer of Read Buffer Descriptors
         * (RBDs)
         */
        struct iwl_dma_ptr *rxq_bd_buf = allocate_dma_buf(free_size * rxq->queue_size, DMA_BIT_MASK(trans_pcie->addr_size));
        rxq->bd_mem_buf = rxq_bd_buf;
        rxq->bd = rxq_bd_buf->addr;
        rxq->bd_dma = rxq_bd_buf->dma;
        bzero(rxq->bd, free_size * rxq->queue_size);
        
        if (trans->cfg->mq_rx_supported) {
            struct iwl_dma_ptr *used_bd_buf = allocate_dma_buf(sizeof(__le32) * rxq->queue_size, DMA_BIT_MASK(trans_pcie->addr_size));
            rxq->used_bd_buf = used_bd_buf;
            rxq->used_bd = (__le32 *)used_bd_buf->addr;
            rxq->used_bd_dma = used_bd_buf->dma;
            bzero(rxq->used_bd, sizeof(__le32) * rxq->queue_size);
        }
        
        /*Allocate the driver's pointer to receive buffer status */
        struct iwl_dma_ptr *rxq_rb_stts_buf = allocate_dma_buf(sizeof(*rxq->rb_stts), DMA_BIT_MASK(trans_pcie->addr_size));
        rxq->rb_stts_buf = rxq_rb_stts_buf;
        rxq->rb_stts = (struct iwl_rb_status*)rxq_rb_stts_buf->addr;
        rxq->rb_stts_dma = rxq_rb_stts_buf->dma;
        bzero(rxq->rb_stts, sizeof(struct iwl_rb_status));
        
        if (!rxq->rb_stts) {
            goto err;
        }
        
    }
    return 0;
    
err:
    for (i = 0; i < trans->num_rx_queues; i++) {
        struct iwl_rxq *rxq = &trans_pcie->rxq[i];

        if (rxq->bd) {
            free_dma_buf(rxq->bd_mem_buf);
        }
        
        rxq->bd_dma = 0;
        rxq->bd = NULL;

        if (rxq->rb_stts) {
            free_dma_buf(rxq->rb_stts_buf);
        }
            

        if (rxq->used_bd) {
            free_dma_buf(rxq->used_bd_buf);
        }
            
        rxq->used_bd_dma = 0;
        rxq->used_bd = NULL;
    }
    iwh_free(trans_pcie->rxq);
    
    return -ENOMEM;
}

// line 693
static void iwl_pcie_rx_hw_init(struct iwl_trans *trans, struct iwl_rxq *rxq)
{
    struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
    u32 rb_size;
    IOInterruptState state;
    const u32 rfdnlog = RX_QUEUE_SIZE_LOG; /* 256 RBDs */
    
    switch (trans_pcie->rx_buf_size) {
        case IWL_AMSDU_4K:
            rb_size = FH_RCSR_RX_CONFIG_REG_VAL_RB_SIZE_4K;
            break;
        case IWL_AMSDU_8K:
            rb_size = FH_RCSR_RX_CONFIG_REG_VAL_RB_SIZE_8K;
            break;
        case IWL_AMSDU_12K:
            rb_size = FH_RCSR_RX_CONFIG_REG_VAL_RB_SIZE_12K;
            break;
        default:
            //WARN_ON(1);
            rb_size = FH_RCSR_RX_CONFIG_REG_VAL_RB_SIZE_4K;
    }
    
    if (!iwl_trans_grab_nic_access(trans, &state))
        return;
    
    /* Stop Rx DMA */
    iwl_write32(trans, FH_MEM_RCSR_CHNL0_CONFIG_REG, 0);
    /* reset and flush pointers */
    iwl_write32(trans, FH_MEM_RCSR_CHNL0_RBDCB_WPTR, 0);
    iwl_write32(trans, FH_MEM_RCSR_CHNL0_FLUSH_RB_REQ, 0);
    iwl_write32(trans, FH_RSCSR_CHNL0_RDPTR, 0);
    
    /* Reset driver's Rx queue write index */
    iwl_write32(trans, FH_RSCSR_CHNL0_RBDCB_WPTR_REG, 0);
    
    /* Tell device where to find RBD circular buffer in DRAM */
    iwl_write32(trans, FH_RSCSR_CHNL0_RBDCB_BASE_REG, (u32)(rxq->bd_dma >> 8));
    
    /* Tell device where in DRAM to update its Rx status */
    iwl_write32(trans, FH_RSCSR_CHNL0_STTS_WPTR_REG, (u32)(rxq->rb_stts_dma >> 4));
    
    /* Enable Rx DMA
     * FH_RCSR_CHNL0_RX_IGNORE_RXF_EMPTY is set because of HW bug in
     *      the credit mechanism in 5000 HW RX FIFO
     * Direct rx interrupts to hosts
     * Rx buffer size 4 or 8k or 12k
     * RB timeout 0x10
     * 256 RBDs
     */
    iwl_write32(trans, FH_MEM_RCSR_CHNL0_CONFIG_REG,
                FH_RCSR_RX_CONFIG_CHNL_EN_ENABLE_VAL |
                FH_RCSR_CHNL0_RX_IGNORE_RXF_EMPTY |
                FH_RCSR_CHNL0_RX_CONFIG_IRQ_DEST_INT_HOST_VAL |
                rb_size |
                (RX_RB_TIMEOUT << FH_RCSR_RX_CONFIG_REG_IRQ_RBTH_POS) |
                (rfdnlog << FH_RCSR_RX_CONFIG_RBDCB_SIZE_POS));
    
    /* Set interrupt coalescing timer to default (2048 usecs) */
    iwl_write8(trans, CSR_INT_COALESCING, IWL_HOST_INT_TIMEOUT_DEF);
    
    /* W/A for interrupt coalescing bug in 7260 and 3160 */
    if (trans->cfg->host_interrupt_operation_mode)
        iwl_set_bit(trans, CSR_INT_COALESCING, IWL_HOST_INT_OPER_MODE);
    
    iwl_trans_release_nic_access(trans, &state);
}

// line 762
void iwl_pcie_enable_rx_wake(struct iwl_trans *trans, bool enable)
{
    if (trans->cfg->device_family != IWL_DEVICE_FAMILY_9000)
        return;
    
    if (CSR_HW_REV_STEP(trans->hw_rev) != SILICON_A_STEP)
        return;
    
    if (!trans->cfg->integrated)
        return;
    
    /*
     * Turn on the chicken-bits that cause MAC wakeup for RX-related
     * values.
     * This costs some power, but needed for W/A 9000 integrated A-step
     * bug where shadow registers are not in the retention list and their
     * value is lost when NIC powers down
     */
    iwl_set_bit(trans, CSR_MAC_SHADOW_REG_CTRL, CSR_MAC_SHADOW_REG_CTRL_RX_WAKE);
    iwl_set_bit(trans, CSR_MAC_SHADOW_REG_CTL2, CSR_MAC_SHADOW_REG_CTL2_RX_WAKE);
}

// line 786
static void iwl_pcie_rx_mq_hw_init(struct iwl_trans *trans)
{
    struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
    u32 rb_size, enabled = 0;
    IOInterruptState state;
    int i;
    
    switch (trans_pcie->rx_buf_size) {
        case IWL_AMSDU_4K:
            rb_size = RFH_RXF_DMA_RB_SIZE_4K;
            break;
        case IWL_AMSDU_8K:
            rb_size = RFH_RXF_DMA_RB_SIZE_8K;
            break;
        case IWL_AMSDU_12K:
            rb_size = RFH_RXF_DMA_RB_SIZE_12K;
            break;
        default:
            rb_size = RFH_RXF_DMA_RB_SIZE_4K;
    }
    
    if (!iwl_trans_grab_nic_access(trans, &state))
        return;
    
    /* Stop Rx DMA */
    iwl_write_prph_no_grab(trans, RFH_RXF_DMA_CFG, 0);
    /* disable free amd used rx queue operation */
    iwl_write_prph_no_grab(trans, RFH_RXF_RXQ_ACTIVE, 0);
    
    for (i = 0; i < trans->num_rx_queues; i++) {
        /* Tell device where to find RBD free table in DRAM */
        iwl_write_prph64_no_grab(trans, RFH_Q_FRBDCB_BA_LSB(i), trans_pcie->rxq[i].bd_dma);
        
        /* Tell device where to find RBD used table in DRAM */
        iwl_write_prph64_no_grab(trans, RFH_Q_URBDCB_BA_LSB(i), trans_pcie->rxq[i].used_bd_dma);
        
        /* Tell device where in DRAM to update its Rx status */
        iwl_write_prph64_no_grab(trans, RFH_Q_URBD_STTS_WPTR_LSB(i), trans_pcie->rxq[i].rb_stts_dma);
        
        /* Reset device indice tables */
        iwl_write_prph_no_grab(trans, RFH_Q_FRBDCB_WIDX(i), 0);
        iwl_write_prph_no_grab(trans, RFH_Q_FRBDCB_RIDX(i), 0);
        iwl_write_prph_no_grab(trans, RFH_Q_URBDCB_WIDX(i), 0);
        
        enabled |= BIT(i) | BIT(i + 16);
    }
    
    /*
     * Enable Rx DMA
     * Rx buffer size 4 or 8k or 12k
     * Min RB size 4 or 8
     * Drop frames that exceed RB size
     * 512 RBDs
     */
    iwl_write_prph_no_grab(trans, RFH_RXF_DMA_CFG,
                           RFH_DMA_EN_ENABLE_VAL | rb_size |
                           RFH_RXF_DMA_MIN_RB_4_8 |
                           RFH_RXF_DMA_DROP_TOO_LARGE_MASK |
                           RFH_RXF_DMA_RBDCB_SIZE_512);
    
    /*
     * Activate DMA snooping.
     * Set RX DMA chunk size to 64B for IOSF and 128B for PCIe
     * Default queue is 0
     */
    iwl_write_prph_no_grab(trans, RFH_GEN_CFG,
                           (u32)(RFH_GEN_CFG_RFH_DMA_SNOOP |
                           RFH_GEN_CFG_VAL(DEFAULT_RXQ_NUM, 0) |
                           RFH_GEN_CFG_SERVICE_DMA_SNOOP |
                           RFH_GEN_CFG_VAL(RB_CHUNK_SIZE,
                                           trans->cfg->integrated ?
                                           RFH_GEN_CFG_RB_CHUNK_SIZE_64 :
                                           RFH_GEN_CFG_RB_CHUNK_SIZE_128)));
    /* Enable the relevant rx queues */
    iwl_write_prph_no_grab(trans, RFH_RXF_RXQ_ACTIVE, enabled);
    
    iwl_trans_release_nic_access(trans, &state);
    
    /* Set interrupt coalescing timer to default (2048 usecs) */
    iwl_write8(trans, CSR_INT_COALESCING, IWL_HOST_INT_TIMEOUT_DEF);
    
    iwl_pcie_enable_rx_wake(trans, true);
}

// line 874
static void iwl_pcie_rx_init_rxb_lists(struct iwl_rxq *rxq)
{
    //lockdep_assert_held(&rxq->lock);
    
    TAILQ_INIT(&rxq->rx_free);
    TAILQ_INIT(&rxq->rx_used);
    rxq->free_count = 0;
    rxq->used_count = 0;
}

// line 890
static int _iwl_pcie_rx_init(struct iwl_trans *trans)
{
    struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
    struct iwl_rxq *def_rxq;
    struct iwl_rb_allocator *rba = &trans_pcie->rba;
    int i, err, queue_size, allocator_pool_size, num_alloc;
    
    if (!trans_pcie->rxq) {
        err = iwl_pcie_rx_alloc(trans);
        if (err)
            return err;
    }
    def_rxq = trans_pcie->rxq;

    //IOSimpleLockLock(rba->lock);
    rba->req_pending = 0;
    rba->req_ready = 0;
    
    TAILQ_INIT(&rba->rbd_allocated);
    TAILQ_INIT(&rba->rbd_empty);
    //IOSimpleLockUnlock(rba->lock);
    
    /* free all first - we might be reconfigured for a different size */
    iwl_pcie_free_rbs_pool(trans);
    
    for (i = 0; i < RX_QUEUE_SIZE; i++)
        def_rxq->queue[i] = NULL;
    
    for (i = 0; i < trans->num_rx_queues; i++) {
        struct iwl_rxq *rxq = &trans_pcie->rxq[i];
        
        rxq->id = i;
        
        //IOSimpleLockLock(rxq->lock);
        /*
         * Set read write pointer to reflect that we have processed
         * and used all buffers, but have not restocked the Rx queue
         * with fresh buffers
         */
        rxq->read = 0;
        rxq->write = 0;
        rxq->write_actual = 0;
        memset(rxq->rb_stts, 0, sizeof(*rxq->rb_stts));
        
        iwl_pcie_rx_init_rxb_lists(rxq);
        
        //IOSimpleLockUnlock(rxq->lock);
    }
    
    /* move the pool to the default queue and allocator ownerships */
    queue_size = trans->cfg->mq_rx_supported ? MQ_RX_NUM_RBDS : RX_QUEUE_SIZE;
    allocator_pool_size = trans->num_rx_queues * (RX_CLAIM_REQ_ALLOC - RX_POST_REQ_ALLOC);
    num_alloc = queue_size + allocator_pool_size;
    for (i = 0; i < num_alloc; i++) {
        struct iwl_rx_mem_buffer *rxb = &trans_pcie->rx_pool[i];
        
        if (i < allocator_pool_size)
            TAILQ_INSERT_HEAD(&rba->rbd_empty, rxb, list);
        else
            TAILQ_INSERT_HEAD(&def_rxq->rx_used, rxb, list);
        
        trans_pcie->global_table[i] = rxb;
        rxb->vid = (u16)(i + 1);
        rxb->invalid = true;
    }
    
    iwl_pcie_rxq_alloc_rbs(trans, def_rxq);
    
    return 0;
}

// line 967
int iwl_pcie_rx_init(struct iwl_trans *trans)
{
    struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
    int ret = _iwl_pcie_rx_init(trans);
    
    if (ret)
        return ret;
    
    if (trans->cfg->mq_rx_supported)
        iwl_pcie_rx_mq_hw_init(trans);
    else
        iwl_pcie_rx_hw_init(trans, trans_pcie->rxq);
    
    iwl_pcie_rxq_restock(trans, trans_pcie->rxq);
    
    //IOSimpleLockLock(trans_pcie->rxq->lock);
    iwl_pcie_rxq_inc_wr_ptr(trans, trans_pcie->rxq);
    //IOSimpleLockUnlock(trans_pcie->rxq->lock);
    
    return 0;
}

// line 989
int iwl_pcie_gen2_rx_init(struct iwl_trans *trans)
{
    /*
     * We don't configure the RFH.
     * Restock will be done at alive, after firmware configured the RFH.
     */
    return _iwl_pcie_rx_init(trans);
}

// line 998
void iwl_pcie_rx_free(struct iwl_trans *trans)
{
    struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
    //    struct iwl_rb_allocator *rba = &trans_pcie->rba;
    //    int free_size = trans->cfg->mq_rx_supported ? sizeof(__le64) : sizeof(__le32);
    int i;
    
    /*
     * if rxq is NULL, it means that nothing has been allocated,
     * exit now
     */
    if (!trans_pcie->rxq) {
        IWL_DEBUG_INFO(trans, "Free NULL rx context\n");
        return;
    }
    
    //cancel_work_sync(&rba->rx_alloc);
    
    iwl_pcie_free_rbs_pool(trans);
    
    for (i = 0; i < trans->num_rx_queues; i++) {
        struct iwl_rxq *rxq = &trans_pcie->rxq[i];
        
        //        if (rxq->bd)
        //            dma_free_coherent(trans->dev,
        //                              free_size * rxq->queue_size,
        //                              rxq->bd, rxq->bd_dma);
        rxq->bd_dma = 0;
        rxq->bd = NULL;
        
        //        if (rxq->rb_stts)
        //            dma_free_coherent(trans->dev,
        //                              sizeof(struct iwl_rb_status),
        //                              rxq->rb_stts, rxq->rb_stts_dma);
        //        else
        //            IWL_DEBUG_INFO(trans,
        //                           "Free rxq->rb_stts which is NULL\n");
        
        //        if (rxq->used_bd)
        //            dma_free_coherent(trans->dev,
        //                              sizeof(__le32) * rxq->queue_size,
        //                              rxq->used_bd, rxq->used_bd_dma);
        rxq->used_bd_dma = 0;
        rxq->used_bd = NULL;
        
        //        if (rxq->napi.poll)
        //            netif_napi_del(&rxq->napi);
    }
    iwh_free(trans_pcie->rxq);
}

/* line 1050
 * iwl_pcie_rx_reuse_rbd - Recycle used RBDs
 *
 * Called when a RBD can be reused. The RBD is transferred to the allocator.
 * When there are 2 empty RBDs - a request for allocation is posted
 */
static void iwl_pcie_rx_reuse_rbd(struct iwl_trans *trans, struct iwl_rx_mem_buffer *rxb, struct iwl_rxq *rxq,
                                  bool emergency)
{
    struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
    struct iwl_rb_allocator *rba = &trans_pcie->rba;
    
    /* Move the RBD to the used list, will be moved to allocator in batches
     * before claiming or posting a request*/
    TAILQ_INSERT_TAIL(&rxq->rx_used, rxb, list);
    
    if (unlikely(emergency))
        return;
    
    /* Count the allocator owned RBDs */
    rxq->used_count++;
    
    /* If we have RX_POST_REQ_ALLOC new released rx buffers -
     * issue a request for allocator. Modulo RX_CLAIM_REQ_ALLOC is
     * used for the case we failed to claim RX_CLAIM_REQ_ALLOC,
     * after but we still need to post another request.
     */
    if ((rxq->used_count % RX_CLAIM_REQ_ALLOC) == RX_POST_REQ_ALLOC) {
        /* Move the 2 RBDs to the allocator ownership.
         Allocator has another 6 from pool for the request completion*/
        //IOSimpleLockLock(rba->lock);
        if (!TAILQ_EMPTY(&rxq->rx_used)) {
            TAILQ_CONCAT(&rba->rbd_empty, &rxq->rx_used, list);
            TAILQ_INIT(&rxq->rx_used);
        }
        //IOSimpleLockUnlock(rba->lock);
        
        OSIncrementAtomic(&rba->req_pending);
        // TODO: Implement
        iwl_pcie_rx_allocator(trans);
        //queue_work(rba->alloc_wq, &rba->rx_alloc);
    }
}

// line 1090
void IntelWifi::iwl_pcie_rx_handle_rb(struct iwl_trans *trans, struct iwl_rxq *rxq, struct iwl_rx_mem_buffer *rxb,
                                      bool emergency)
{
    struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
    struct iwl_txq *txq = trans_pcie->txq[trans_pcie->cmd_queue];
    bool page_stolen = false;
    unsigned int max_len = PAGE_SIZE << trans_pcie->rx_page_order;
    u32 offset = 0;
    
    if (WARN_ON(!rxb))
        return;
    
    //dma_unmap_page(trans->dev, rxb->page_dma, max_len, DMA_FROM_DEVICE);
    
    while (offset + sizeof(u32) + sizeof(struct iwl_cmd_header) < max_len) {
        struct iwl_rx_packet *pkt;
        u16 sequence;
        bool reclaim;
        int index, cmd_index, len;

        struct iwl_rx_cmd_buffer rxcb = {
            ._offset = (int)offset,
            ._rx_page_order = trans_pcie->rx_page_order,
            ._page = rxb->page,
            ._page_stolen = false,
            .truesize = max_len,
        };
        
        pkt = (struct iwl_rx_packet *)rxb_addr(&rxcb);

        if (pkt->len_n_flags == cpu_to_le32(FH_RSCSR_FRAME_INVALID)) {
            IWL_DEBUG_RX(trans, "Q %d: RB end marker at offset %d\n", rxq->id, offset);
            break;
        }
        
        u32 frame_queue = (le32_to_cpu(pkt->len_n_flags) & FH_RSCSR_RXQ_MASK) >> FH_RSCSR_RXQ_POS;
        
        if (frame_queue != rxq->id) {
            IWL_DEBUG_RX(trans, "frame on invalid queue - is on %d and indicates %d\n", rxq->id, frame_queue);
        }
    
        IWL_DEBUG_RX(trans,
                     "Q %d: cmd at offset %d: %s (%.2x.%2x, seq 0x%x)\n",
                     rxq->id, offset,
                     iwl_get_cmd_string(trans, iwl_cmd_id(pkt->hdr.cmd, pkt->hdr.group_id, 0)),
                     pkt->hdr.group_id, pkt->hdr.cmd,
                     le16_to_cpu(pkt->hdr.sequence));
        
        len = iwl_rx_packet_len(pkt);
        len += sizeof(u32); /* account for status word */

        //        trace_iwlwifi_dev_rx(trans->dev, trans, pkt, len);
        //        trace_iwlwifi_dev_rx_data(trans->dev, trans, pkt, len);
        
        /* Reclaim a command buffer only if this packet is a response
         *   to a (driver-originated) command.
         * If the packet (e.g. Rx frame) originated from uCode,
         *   there is no command buffer to reclaim.
         * Ucode should set SEQ_RX_FRAME bit if ucode-originated,
         *   but apparently a few don't get set; catch them here. */
        reclaim = !(pkt->hdr.sequence & SEQ_RX_FRAME);
        if (reclaim && !pkt->hdr.group_id) {
            int i;
            
            for (i = 0; i < trans_pcie->n_no_reclaim_cmds; i++) {
                if (trans_pcie->no_reclaim_cmds[i] == pkt->hdr.cmd) {
                    reclaim = false;
                    break;
                }
            }
        }
        
        sequence = le16_to_cpu(pkt->hdr.sequence);
        index = SEQ_TO_INDEX(sequence);
        cmd_index = iwl_pcie_get_cmd_index(txq, index);
        
        if (rxq->id == 0)
            opmode->rx(NULL, NULL, &rxcb);
        
        // TODO: Implement
        //if (rxq->id == 0)
        //    iwl_op_mode_rx(trans->op_mode, &rxq->napi, &rxcb);
        //else
        //    iwl_op_mode_rx_rss(trans->op_mode, &rxq->napi, &rxcb, rxq->id);
        
        if (reclaim) {
            iwh_free((void *)txq->entries[cmd_index].free_buf);
            txq->entries[cmd_index].free_buf = NULL;
        }
        
        /*
         * After here, we should always check rxcb._page_stolen,
         * if it is true then one of the handlers took the page.
         */
        
        if (reclaim) {
            /* Invoke any callbacks, transfer the buffer to caller,
             * and fire off the (possibly) blocking
             * iwl_trans_send_cmd()
             * as we reclaim the driver command queue */
            if (!rxcb._page_stolen)
                iwl_pcie_hcmd_complete(trans, &rxcb);
            else
                IWL_WARN(trans, "Claim null rxb?\n");
        }
        
        page_stolen |= rxcb._page_stolen;
        offset += LNX_ALIGN(len, FH_RSCSR_FRAME_ALIGN);
    }
    
//    IOBufferMemoryDescriptor *page = static_cast<IOBufferMemoryDescriptor *>(rxb->page);
    
    /* page was stolen from us -- free our reference */
    if (page_stolen) {
        iwl_free_packet(trans, rxb->page);
        rxb->page = NULL;
    }
    
    /* Reuse the page if possible. For notification packets and
     * SKBs that fail to Rx correctly, add them back into the
     * rx_free list for reuse later. */
    if (rxb->page != NULL) {
        rxb->page_dma = iwl_dmamap_mbuf(trans, rxb->page);
        if (!rxb->page_dma) {
            /*
             * free the page(s) as well to not break
             * the invariant that the items on the used
             * list have no page(s)
             */
            //__free_pages(rxb->page, trans_pcie->rx_page_order);
            rxb->page = NULL;
            iwl_pcie_rx_reuse_rbd(trans, rxb, rxq, emergency);
        } else {
            TAILQ_INSERT_TAIL(&rxq->rx_free, rxb, list);
            rxq->free_count++;
        }
    } else {
        iwl_pcie_rx_reuse_rbd(trans, rxb, rxq, emergency);
    }
}

/* line 1236
 * iwl_pcie_rx_handle - Main entry function for receiving responses from fw
 */
void IntelWifi::iwl_pcie_rx_handle(struct iwl_trans *trans, int queue)
{
    struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
    struct iwl_rxq *rxq = &trans_pcie->rxq[queue];
    u32 r, i, count = 0;
    bool emergency = false;
    
restart:
    //IOSimpleLockLock(rxq->lock);
    /* uCode's read index (stored in shared DRAM) indicates the last Rx
     * buffer that the driver may process (last buffer filled by ucode). */
    r = le16_to_cpu(rxq->rb_stts->closed_rb_num) & 0x0FFF;
    i = rxq->read;
    
    /* W/A 9000 device step A0 wrap-around bug */
    r &= (rxq->queue_size - 1);
    
    /* Rx interrupt, but nothing sent from uCode */
    if (i == r)
        IWL_DEBUG_RX(trans, "Q %d: HW = SW = %d\n", rxq->id, r);
    
    while (i != r) {
        struct iwl_rx_mem_buffer *rxb;
        
        if (rxq->used_count == rxq->queue_size / 2)
            emergency = true;
        
        if (trans->cfg->mq_rx_supported) {
            /*
             * used_bd is a 32 bit but only 12 are used to retrieve
             * the vid
             */
            u16 vid = le32_to_cpu(rxq->used_bd[i]) & 0x0FFF;
            
            if ((!vid || vid > ARRAY_SIZE(trans_pcie->global_table))) {
                IWL_ERR(trans, "Invalid rxb index from HW %u\n", (u32)vid);
                iwl_force_nmi(trans);
                goto out;
            }
            rxb = trans_pcie->global_table[vid - 1];
            if (rxb->invalid) {
                IWL_ERR(trans, "Invalid rxb from HW %u\n", (u32)vid);
                iwl_force_nmi(trans);
                goto out;
            }
            rxb->invalid = true;
        } else {
            rxb = rxq->queue[i];
            rxq->queue[i] = NULL;
        }
        
        IWL_DEBUG_RX(trans, "Q %d: HW = %d, SW = %d\n", rxq->id, r, i);
        iwl_pcie_rx_handle_rb(trans, rxq, rxb, emergency);
        
        i = (i + 1) & (rxq->queue_size - 1);
        
        /*
         * If we have RX_CLAIM_REQ_ALLOC released rx buffers -
         * try to claim the pre-allocated buffers from the allocator.
         * If not ready - will try to reclaim next time.
         * There is no need to reschedule work - allocator exits only
         * on success
         */
        if (rxq->used_count >= RX_CLAIM_REQ_ALLOC)
            iwl_pcie_rx_allocator_get(trans, rxq);
        
        if (rxq->used_count % RX_CLAIM_REQ_ALLOC == 0 && !emergency) {
            struct iwl_rb_allocator *rba = &trans_pcie->rba;
            
            /* Add the remaining empty RBDs for allocator use */
            //IOSimpleLockLock(rba->lock);
            if (!TAILQ_EMPTY(&rxq->rx_used)) {
                TAILQ_CONCAT(&rba->rbd_empty, &rxq->rx_used, list);
                TAILQ_INIT(&rxq->rx_used);
            }
            //IOSimpleLockUnlock(rba->lock);
        } else if (emergency) {
            count++;
            if (count == 8) {
                count = 0;
                if (rxq->used_count < rxq->queue_size / 3)
                    emergency = false;
                
                rxq->read = i;
                //IOSimpleLockUnlock(rxq->lock);
                iwl_pcie_rxq_alloc_rbs(trans, rxq);
                iwl_pcie_rxq_restock(trans, rxq);
                goto restart;
            }
        }
    }
out:
    /* Backtrack one entry */
    rxq->read = i;
    //IOSimpleLockUnlock(rxq->lock);
    
    /*
     * handle a case where in emergency there are some unallocated RBDs.
     * those RBDs are in the used list, but are not tracked by the queue's
     * used_count which counts allocator owned RBDs.
     * unallocated emergency RBDs must be allocated on exit, otherwise
     * when called again the function may not be in emergency mode and
     * they will be handed to the allocator with no tracking in the RBD
     * allocator counters, which will lead to them never being claimed back
     * by the queue.
     * by allocating them here, they are now in the queue free list, and
     * will be restocked by the next call of iwl_pcie_rxq_restock.
     */
    if (unlikely(emergency && count))
        iwl_pcie_rxq_alloc_rbs(trans, rxq);
    
    iwl_pcie_rxq_restock(trans, rxq);
}

/* line 1404
 * iwl_pcie_irq_handle_error - called for HW or SW error interrupt from card
 */
void IntelWifi::iwl_pcie_irq_handle_error(struct iwl_trans *trans)
{
    DebugLog("Handle error\n");
    
    struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
    int i;
    
    /* W/A for WiFi/WiMAX coex and WiMAX own the RF */
    if (trans->cfg->internal_wimax_coex && !trans->cfg->apmg_not_supported &&
        (!(iwl_read_prph(trans, APMG_CLK_CTRL_REG) & APMS_CLK_VAL_MRB_FUNC_MODE) ||
          (iwl_read_prph(trans, APMG_PS_CTRL_REG) & APMG_PS_CTRL_VAL_RESET_REQ))) {
             clear_bit(STATUS_SYNC_HCMD_ACTIVE, &trans->status);
             // TODO: Implement
             //iwl_op_mode_wimax_active(trans->op_mode);
             IOLockLock(trans_pcie->wait_command_queue);
             IOLockWakeup(trans_pcie->wait_command_queue, &trans->status, true);
             IOLockUnlock(trans_pcie->wait_command_queue);
             return;
         }
    
    
    for (i = 0; i < trans->cfg->base_params->num_of_queues; i++) {
        if (!trans_pcie->txq[i])
            continue;
        // TODO: Implement
        //del_timer(&trans_pcie->txq[i]->stuck_timer);
    }
    
    /* The STATUS_FW_ERROR bit is set in this function. This must happen
     * before we wake up the command caller, to ensure a proper cleanup. */
    // TODO: Implement. Inside trans->ops->op is used which is undefined and will cause kernel panic
    // iwl_trans_fw_error(trans);
    
    clear_bit(STATUS_SYNC_HCMD_ACTIVE, &trans->status);
    
    IOLockLock(trans_pcie->wait_command_queue);
    IOLockWakeup(trans_pcie->wait_command_queue, &trans->status, true);
    IOLockUnlock(trans_pcie->wait_command_queue);
}

// line 1439
static u32 iwl_pcie_int_cause_non_ict(struct iwl_trans *trans)
{
    u32 inta;
    
    // lockdep_assert_held(&IWL_TRANS_GET_PCIE_TRANS(trans)->irq_lock);
    // trace_iwlwifi_dev_irq(trans->dev);
    
    /* Discover which interrupts are active/pending */
    inta = iwl_read32(trans, CSR_INT);
    
    /* the thread will service interrupts and re-enable them */
    return inta;
}

/* a device (PCI-E) page is 4096 bytes long */
#define ICT_SHIFT   12
#define ICT_SIZE    (1 << ICT_SHIFT)
#define ICT_COUNT   (ICT_SIZE / sizeof(u32))

/* line 1459
 * interrupt handler using ict table, with this interrupt driver will
 * stop using INTA register to get device's interrupt, reading this register
 * is expensive, device will write interrupts in ICT dram table, increment
 * index then will fire interrupt to driver, driver will OR all ICT table
 * entries from current index up to table entry with 0 value. the result is
 * the interrupt we need to service, driver will set the entries back to 0 and
 * set index.
 */
static u32 iwl_pcie_int_cause_ict(struct iwl_trans *trans)
{
    struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
    u32 inta;
    u32 val = 0;
    u32 read;
    
    //trace_iwlwifi_dev_irq(trans->dev);
    
    /* Ignore interrupt if there's nothing in NIC to service.
     * This may be due to IRQ shared with another device,
     * or due to sporadic interrupts thrown from our NIC. */
    read = le32_to_cpu(trans_pcie->ict_tbl[trans_pcie->ict_index]);
    //trace_iwlwifi_dev_ict_read(trans->dev, trans_pcie->ict_index, read);
    if (!read)
        return 0;
    
    /*
     * Collect all entries up to the first 0, starting from ict_index;
     * note we already read at ict_index.
     */
    do {
        val |= read;
        IWL_DEBUG_ISR(trans, "ICT index %d value 0x%08X\n", trans_pcie->ict_index, read);
        trans_pcie->ict_tbl[trans_pcie->ict_index] = 0;
        trans_pcie->ict_index = ((trans_pcie->ict_index + 1) & (ICT_COUNT - 1));
        
        read = le32_to_cpu(trans_pcie->ict_tbl[trans_pcie->ict_index]);
        //        trace_iwlwifi_dev_ict_read(trans->dev, trans_pcie->ict_index,
        //                                   read);
    } while (read);
    
    /* We should not get this value, just ignore it. */
    if (val == 0xffffffff)
        val = 0;
    
    /*
     * this is a w/a for a h/w bug. the h/w bug may cause the Rx bit
     * (bit 15 before shifting it to 31) to clear when using interrupt
     * coalescing. fortunately, bits 18 and 19 stay set when this happens
     * so we use them to decide on the real state of the Rx bit.
     * In order words, bit 15 is set if bit 18 or bit 19 are set.
     */
    if (val & 0xC0000)
        val |= 0x8000;
    
    inta = (0xff & val) | ((0xff00 & val) << 16);
    return inta;
}

// line 1519
void IntelWifi::iwl_pcie_handle_rfkill_irq(struct iwl_trans *trans)
{
    struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
    struct isr_statistics *isr_stats = &trans_pcie->isr_stats;
    bool hw_rfkill, prev, report;
    
    IOLockLock(trans_pcie->mutex);
    prev = test_bit(STATUS_RFKILL_OPMODE, &trans->status);
    hw_rfkill = iwl_is_rfkill_set(trans);
    if (hw_rfkill) {
        set_bit(STATUS_RFKILL_OPMODE, &trans->status);
        set_bit(STATUS_RFKILL_HW, &trans->status);
    }
    if (trans_pcie->opmode_down)
        report = hw_rfkill;
    else
        report = test_bit(STATUS_RFKILL_OPMODE, &trans->status);
    
    IWL_WARN(trans, "RF_KILL bit toggled to %s.\n", hw_rfkill ? "disable radio" : "enable radio");
    
    isr_stats->rfkill++;
    
    if (prev != report)
        iwl_trans_pcie_rf_kill(trans, report);
    
    IOLockUnlock(trans_pcie->mutex);
    
    if (hw_rfkill) {
        if (test_and_clear_bit(STATUS_SYNC_HCMD_ACTIVE, &trans->status))
            IWL_DEBUG_RF_KILL(trans, "Rfkill while SYNC HCMD in flight\n");
        IOLockLock(trans_pcie->wait_command_queue);
        IOLockWakeup(trans_pcie->wait_command_queue, &trans->status, true);
        IOLockUnlock(trans_pcie->wait_command_queue);
    } else {
        clear_bit(STATUS_RFKILL_HW, &trans->status);
        if (trans_pcie->opmode_down)
            clear_bit(STATUS_RFKILL_OPMODE, &trans->status);
    }
}

// line 1559
void IntelWifi::iwl_pcie_irq_handler(int irq, void *dev_id)
{
    struct iwl_trans *trans = (struct iwl_trans *)dev_id;
    struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
    struct isr_statistics *isr_stats = &trans_pcie->isr_stats;
    u32 inta = 0;
    u32 handled = 0;
    
    // lock_map_acquire(&trans->sync_cmd_lockdep_map);
    
    //IOSimpleLockLock(trans_pcie->irq_lock);
    
    /* dram interrupt table not set yet,
     * use legacy interrupt.
     */
    if (trans_pcie->use_ict)
        inta = iwl_pcie_int_cause_ict(trans);
    else
        inta = iwl_pcie_int_cause_non_ict(trans);
    
    if (iwl_have_debug_level(IWL_DL_ISR)) {
        IWL_DEBUG_ISR(trans,
                      "ISR inta 0x%08x, enabled 0x%08x(sw), enabled(hw) 0x%08x, fh 0x%08x\n",
                      inta, trans_pcie->inta_mask,
                      iwl_read32(trans, CSR_INT_MASK),
                      iwl_read32(trans, CSR_FH_INT_STATUS));
        if (inta & (~trans_pcie->inta_mask))
            IWL_DEBUG_ISR(trans, "We got a masked interrupt (0x%08x)\n", inta & (~trans_pcie->inta_mask));
    }
    
    inta &= trans_pcie->inta_mask;
    
    /*
     * Ignore interrupt if there's nothing in NIC to service.
     * This may be due to IRQ shared with another device,
     * or due to sporadic interrupts thrown from our NIC.
     */
    if (!inta) {
        IWL_DEBUG_ISR(trans, "Ignore interrupt, inta == 0\n");
        /*
         * Re-enable interrupts here since we don't
         * have anything to service
         */
        if (test_bit(STATUS_INT_ENABLED, &trans->status))
            _iwl_enable_interrupts(trans);
        
        //IOSimpleLockUnlock(trans_pcie->irq_lock);
        //lock_map_release(&trans->sync_cmd_lockdep_map);
        return;
    }
    
    if (unlikely(inta == 0xFFFFFFFF || (inta & 0xFFFFFFF0) == 0xa5a5a5a0)) {
        /*
         * Hardware disappeared. It might have
         * already raised an interrupt.
         */
        IWL_WARN(trans, "HARDWARE GONE?? INTA == 0x%08x\n", inta);
        //IOSimpleLockUnlock(trans_pcie->irq_lock);
        goto out;
    }
    
    /* Ack/clear/reset pending uCode interrupts.
     * Note:  Some bits in CSR_INT are "OR" of bits in CSR_FH_INT_STATUS,
     */
    /* There is a hardware bug in the interrupt mask function that some
     * interrupts (i.e. CSR_INT_BIT_SCD) can still be generated even if
     * they are disabled in the CSR_INT_MASK register. Furthermore the
     * ICT interrupt handling mechanism has another bug that might cause
     * these unmasked interrupts fail to be detected. We workaround the
     * hardware bugs here by ACKing all the possible interrupts so that
     * interrupt coalescing can still be achieved.
     */
    iwl_write32(trans, CSR_INT, inta | ~trans_pcie->inta_mask);
    
    if (iwl_have_debug_level(IWL_DL_ISR))
        IWL_DEBUG_ISR(trans, "inta 0x%08x, enabled 0x%08x\n", inta, iwl_read32(trans, CSR_INT_MASK));
    
    //IOSimpleLockUnlock(trans_pcie->irq_lock);
    
    /* Now service all interrupt bits discovered above. */
    if (inta & CSR_INT_BIT_HW_ERR) {
        IWL_ERR(trans, "Hardware error detected.  Restarting.\n");
        
        /* Tell the device to stop sending interrupts */
        iwl_disable_interrupts(trans);
        
        isr_stats->hw++;
        iwl_pcie_irq_handle_error(trans);
        
        handled |= CSR_INT_BIT_HW_ERR;
        
        goto out;
    }
    
    if (iwl_have_debug_level(IWL_DL_ISR)) {
        /* NIC fires this, but we don't use it, redundant with WAKEUP */
        if (inta & CSR_INT_BIT_SCD) {
            IWL_DEBUG_ISR(trans, "Scheduler finished to transmit the frame/frames.\n");
            isr_stats->sch++;
        }
        
        /* Alive notification via Rx interrupt will do the real work */
        if (inta & CSR_INT_BIT_ALIVE) {
            IWL_DEBUG_ISR(trans, "Alive interrupt\n");
            isr_stats->alive++;
            if (trans->cfg->gen2) {
                /*
                 * We can restock, since firmware configured
                 * the RFH
                 */
                iwl_pcie_rxmq_restock(trans, trans_pcie->rxq);
            }
        }
    }
    
    /* Safely ignore these bits for debug checks below */
    inta &= ~(CSR_INT_BIT_SCD | CSR_INT_BIT_ALIVE);
    
    /* HW RF KILL switch toggled */
    if (inta & CSR_INT_BIT_RF_KILL) {
        iwl_pcie_handle_rfkill_irq(trans);
        handled |= CSR_INT_BIT_RF_KILL;
    }
    
    /* Chip got too hot and stopped itself */
    if (inta & CSR_INT_BIT_CT_KILL) {
        IWL_ERR(trans, "Microcode CT kill error detected.\n");
        isr_stats->ctkill++;
        handled |= CSR_INT_BIT_CT_KILL;
    }
    
    /* Error detected by uCode */
    if (inta & CSR_INT_BIT_SW_ERR) {
        IWL_ERR(trans, "Microcode SW error detected. Restarting 0x%X.\n", inta);
        isr_stats->sw++;
        iwl_pcie_irq_handle_error(trans);
        handled |= CSR_INT_BIT_SW_ERR;
    }
    
    /* uCode wakes up after power-down sleep */
    if (inta & CSR_INT_BIT_WAKEUP) {
        IWL_DEBUG_ISR(trans, "Wakeup interrupt\n");
        
        iwl_pcie_rxq_check_wrptr(trans);
        iwl_pcie_txq_check_wrptrs(trans);
        
        isr_stats->wakeup++;
        
        handled |= CSR_INT_BIT_WAKEUP;
    }
    
    /* All uCode command responses, including Tx command responses,
     * Rx "responses" (frame-received notification), and other
     * notifications from uCode come through here*/
    if (inta & (CSR_INT_BIT_FH_RX | CSR_INT_BIT_SW_RX | CSR_INT_BIT_RX_PERIODIC)) {
        IWL_DEBUG_ISR(trans, "Rx interrupt\n");
        if (inta & (CSR_INT_BIT_FH_RX | CSR_INT_BIT_SW_RX)) {
            handled |= (CSR_INT_BIT_FH_RX | CSR_INT_BIT_SW_RX);
            iwl_write32(trans, CSR_FH_INT_STATUS, CSR_FH_INT_RX_MASK);
        }
        if (inta & CSR_INT_BIT_RX_PERIODIC) {
            handled |= CSR_INT_BIT_RX_PERIODIC;
            iwl_write32(trans, CSR_INT, CSR_INT_BIT_RX_PERIODIC);
        }
        /* Sending RX interrupt require many steps to be done in the
         * the device:
         * 1- write interrupt to current index in ICT table.
         * 2- dma RX frame.
         * 3- update RX shared data to indicate last write index.
         * 4- send interrupt.
         * This could lead to RX race, driver could receive RX interrupt
         * but the shared data changes does not reflect this;
         * periodic interrupt will detect any dangling Rx activity.
         */
        
        /* Disable periodic interrupt; we use it as just a one-shot. */
        iwl_write8(trans, CSR_INT_PERIODIC_REG, CSR_INT_PERIODIC_DIS);
        
        /*
         * Enable periodic interrupt in 8 msec only if we received
         * real RX interrupt (instead of just periodic int), to catch
         * any dangling Rx interrupt.  If it was just the periodic
         * interrupt, there was no dangling Rx activity, and no need
         * to extend the periodic interrupt; one-shot is enough.
         */
        if (inta & (CSR_INT_BIT_FH_RX | CSR_INT_BIT_SW_RX))
            iwl_write8(trans, CSR_INT_PERIODIC_REG, CSR_INT_PERIODIC_ENA);
        
        isr_stats->rx++;
        
        // local_bh_disable();
        iwl_pcie_rx_handle(trans, 0);
        //        local_bh_enable();
    }
    
    /* This "Tx" DMA channel is used only for loading uCode */
    if (inta & CSR_INT_BIT_FH_TX) {
        iwl_write32(trans, CSR_FH_INT_STATUS, CSR_FH_INT_TX_MASK);
        IWL_DEBUG_ISR(trans, "uCode load interrupt\n");
        isr_stats->tx++;
        handled |= CSR_INT_BIT_FH_TX;
        /* Wake up uCode load routine, now that load is complete */
        IOLockLock(trans_pcie->ucode_write_waitq);
        trans_pcie->ucode_write_complete = true;
        IOLockWakeup(trans_pcie->ucode_write_waitq, &trans_pcie->ucode_write_complete, true);
        IOLockUnlock(trans_pcie->ucode_write_waitq);
    }
    
    if (inta & ~handled) {
        IWL_ERR(trans, "Unhandled INTA bits 0x%08x\n", inta & ~handled);
        isr_stats->unhandled++;
    }
    
    if (inta & ~(trans_pcie->inta_mask)) {
        IWL_WARN(trans, "Disabled INTA bits 0x%08x were pending\n", inta & ~trans_pcie->inta_mask);
    }
    
    //IOSimpleLockLock(trans_pcie->irq_lock);
    /* only Re-enable all interrupt if disabled by irq */
    if (test_bit(STATUS_INT_ENABLED, &trans->status))
        _iwl_enable_interrupts(trans);
    /* we are loading the firmware, enable FH_TX interrupt only */
    else if (handled & CSR_INT_BIT_FH_TX)
        iwl_enable_fw_load_int(trans);
    /* Re-enable RF_KILL if it occurred */
    else if (handled & CSR_INT_BIT_RF_KILL)
        iwl_enable_rfkill_int(trans);
    
    //IOSimpleLockUnlock(trans_pcie->irq_lock);
    
out:
    //lock_map_release(&trans->sync_cmd_lockdep_map);
    return;
}

/******************************************************************************
 *
 * ICT functions
 *
 ******************************************************************************/

/* line 1807
 * Free dram table
 */
void iwl_pcie_free_ict(struct iwl_trans *trans)
{
    struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
    
    if (trans_pcie->ict_tbl) {
        free_dma_buf(trans_pcie->ict_dma_buf);
        trans_pcie->ict_tbl = NULL;
        trans_pcie->ict_tbl_dma = 0;
    }
}

/* line 1820
 * allocate dram shared table, it is an aligned memory
 * block of ICT_SIZE.
 * also reset all data related to ICT table interrupt.
 */
int iwl_pcie_alloc_ict(struct iwl_trans *trans)
{
    struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
    
    struct iwl_dma_ptr* buf = allocate_dma_buf(ICT_SIZE, DMA_BIT_MASK(trans_pcie->addr_size));
    
    trans_pcie->ict_dma_buf = buf;
    trans_pcie->ict_tbl = (__le32 *)buf->addr;
    trans_pcie->ict_tbl_dma = buf->dma;
    
    if (!trans_pcie->ict_tbl)
        return -ENOMEM;
    
    bzero((void*)trans_pcie->ict_tbl, ICT_SIZE);
    
    /* just an API sanity check ... it is guaranteed to be aligned */
    if (trans_pcie->ict_tbl_dma & (ICT_SIZE - 1)) {
        iwl_pcie_free_ict(trans);
        return -EINVAL;
    }
    
    return 0;
}

/* line 1845
 * Device is going up inform it about using ICT interrupt table,
 * also we need to tell the driver to start using ICT interrupt.
 */
void iwl_pcie_reset_ict(struct iwl_trans *trans)
{
    struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
    u32 val;
    
    if (!trans_pcie->ict_tbl)
        return;
    
    //IOSimpleLockLock(trans_pcie->irq_lock);
    _iwl_disable_interrupts(trans);
    
    memset(trans_pcie->ict_tbl, 0, ICT_SIZE);
    
    val = (u32)(trans_pcie->ict_tbl_dma >> ICT_SHIFT);
    
    val |= CSR_DRAM_INT_TBL_ENABLE |
           CSR_DRAM_INIT_TBL_WRAP_CHECK |
           CSR_DRAM_INIT_TBL_WRITE_POINTER;
    
    IWL_DEBUG_ISR(trans, "CSR_DRAM_INT_TBL_REG =0x%x\n", val);
    
    iwl_write32(trans, CSR_DRAM_INT_TBL_REG, val);
    trans_pcie->use_ict = true;
    trans_pcie->ict_index = 0;
    iwl_write32(trans, CSR_INT, trans_pcie->inta_mask);
    _iwl_enable_interrupts(trans);
    //IOSimpleLockUnlock(trans_pcie->irq_lock);
}




/* line 1877
 * Device is going down disable ict interrupt usage
 */
void iwl_pcie_disable_ict(struct iwl_trans *trans)
{
    struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
    
    //IOSimpleLockLock(trans_pcie->irq_lock);
    trans_pcie->use_ict = false;
    //IOSimpleLockUnlock(trans_pcie->irq_lock);
}
