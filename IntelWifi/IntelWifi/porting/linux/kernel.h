//
//  kernel.h
//  IntelWifi
//
//  Created by Roman Peshkov on 28/12/2017.
//  Copyright © 2017 Roman Peshkov. All rights reserved.
//

#ifndef kernel_h
#define kernel_h


#include <linux/types.h>
#include <linux/compiler.h>
#include <macro_stubs.h>

#include <sys/errno.h>
#include <libkern/OSTypes.h>
#include <IOKit/IOLib.h>

#define ERFKILL        132    /* Operation not possible due to RF-kill */

#define EHWPOISON    133    /* Memory page has hardware error */


/* Defined for the NFSv3 protocol */
#define EBADHANDLE    521    /* Illegal NFS file handle */
#define ENOTSYNC    522    /* Update synchronization mismatch */
#define EBADCOOKIE    523    /* Cookie is stale */
#define ENOTSUPP    524    /* Operation is not supported */
#define ETOOSMALL    525    /* Buffer or request is too small */
#define ESERVERFAULT    526    /* An untranslatable error occurred */
#define EBADTYPE    527    /* Type not supported by server */
#define EJUKEBOX    528    /* Request initiated, but will not complete before timeout */
#define EIOCBQUEUED    529    /* iocb queued, will get completion event */
#define ERECALLCONFLICT    530    /* conflict with recalled state */


#define min_t(type,x,y) \
({ type __x = (x); type __y = (y); __x < __y ? __x: __y; })

#define max_t(type, x, y) \
({ type __x = (x); type __y = (y); __x > __y ? __x: __y; })





#define likely(x)       OS_EXPECT(!!(x), 1)
#define unlikely(x)     OS_EXPECT(!!(x), 0)

# define do_div(n,base) ({                    \
uint32_t __base = (base);                \
uint32_t __rem;                        \
__rem = ((uint64_t)(n)) % __base;            \
(n) = ((uint64_t)(n)) / __base;                \
__rem;                            \
})



static inline int fls64(UInt64 x)
{
    int bitpos = -1;
    /*
     * AMD64 says BSRQ won't clobber the dest reg if x==0; Intel64 says the
     * dest reg is undefined if x==0, but their CPU architect says its
     * value is written to set it to the same as before.
     */
    asm("bsrq %1,%q0"
        : "+r" (bitpos)
        : "rm" (x));
    return bitpos + 1;
}


/*
 * Runtime evaluation of get_order()
 */
static inline
int __get_order(unsigned long size)
{
    int order;
    
    size--;
    size >>= PAGE_SHIFT;
#if BITS_PER_LONG == 32
    order = linux_fls(size);
#else
    order = fls64(size);
#endif
    return order;
}

/**
 * get_order - Determine the allocation order of a memory size
 * @size: The size for which to get the order
 *
 * Determine the allocation order of a particular sized block of memory.  This
 * is on a logarithmic scale, where:
 *
 *    0 -> 2^0 * PAGE_SIZE and below
 *    1 -> 2^1 * PAGE_SIZE to 2^0 * PAGE_SIZE + 1
 *    2 -> 2^2 * PAGE_SIZE to 2^1 * PAGE_SIZE + 1
 *    3 -> 2^3 * PAGE_SIZE to 2^2 * PAGE_SIZE + 1
 *    4 -> 2^4 * PAGE_SIZE to 2^3 * PAGE_SIZE + 1
 *    ...
 *
 * The order returned is used to find the smallest allocation granule required
 * to hold an object of the specified size.
 *
 * The result is undefined if the size is 0.
 *
 * This function may be used to initialise variables with compile time
 * evaluations of constants.
 */
#define get_order(n)                        \
(                                \
__builtin_constant_p(n) ? (                \
((n) == 0UL) ? BITS_PER_LONG - PAGE_SHIFT :    \
(((n) < (1UL << PAGE_SHIFT)) ? 0 :        \
ilog2((n) - 1) - PAGE_SHIFT + 1)        \
) :                            \
__get_order(n)                        \
)

/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:    the pointer to the member.
 * @type:    the type of the container struct this is embedded in.
 * @member:    the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member) ({                \
pointer_t __mptr = (pointer_t)(ptr);                    \
((type *)(__mptr - offsetof(type, member))); })



static inline void * ERR_PTR(long error)
{
    return (void *) error;
}

/*
 * This looks more complex than it should be. But we need to
 * get the type for the ~ right in round_down (it needs to be
 * as wide as the result!), and we want to evaluate the macro
 * arguments just once each.
 */
#define __round_mask(x, y) ((__typeof__(x))((y)-1))
#define round_up(x, y) ((((x)-1) | __round_mask(x, y))+1)
#define round_down(x, y) ((x) & ~__round_mask(x, y))


#define DMA_BIT_MASK(n)    (((n) == 64) ? ~0ULL : ((1ULL<<(n))-1))


// log2.h
/*
 *  Determine whether some value is a power of two, where zero is
 * *not* considered a power of two.
 */

static inline __attribute__((const))
bool is_power_of_2(unsigned long n)
{
    return (n != 0 && ((n & (n - 1)) == 0));
}

static inline void put_unaligned_le32(u32 val, void *p)
{
    *((__le32 *)p) = cpu_to_le32(val);
}

static inline u32 get_unaligned_le32(const void *p)
{
    return le32_to_cpup((__le32 *)p);
}








#endif /* kernel_h */
