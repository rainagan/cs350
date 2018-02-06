/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include "opt-A3.h"

/*
 * Dumb MIPS-only "VM system" that is intended to only be just barely
 * enough to struggle off the ground.
 */

/* under dumbvm, always have 48k of user stack */
#define DUMBVM_STACKPAGES    12

#if OPT_A3
struct CoreMap {
	/* keep track of where this contiguous block starts */
	paddr_t start_addr;
	/* indicate how many contiguous block allocated */
	int total_block;
	/* what number is this frame in the allocated contiguous block */
	int block_num;
	/* keep track if each frame is used or not */
	bool use;
};
#endif

/*
 * Wrap rma_stealmem in a spinlock.
 */
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

#if OPT_A3
struct CoreMap *core_map;
int total_frames = 0;
#endif

void
vm_bootstrap(void)
{
	/* Do nothing. */
	#if OPT_A3
	/* call ram_getsize to get the remaining physical memory in the system */
	paddr_t lo, hi;
	ram_getsize(&lo, &hi);
//	kprintf("lo = %u, hi = %u\n", lo, hi);

	/* Logically partition the memory into fixed size frames. 
	Each frame is PAGE_SIZE bytes.
	*/
	int num_of_frames = (hi - lo)/PAGE_SIZE; 
//	kprintf("num_of_frames = %d\n", num_of_frames);

	/* Store core map in the start of the memory returned by ram_getsize. */
	core_map = (struct CoreMap *)PADDR_TO_KVADDR(lo);

	/* find total memories that core map can use */
	lo += num_of_frames * sizeof(struct CoreMap);
//	kprintf("lo before round up = %u\n", lo);
	lo = ROUNDUP(lo, PAGE_SIZE);
	kprintf("lo after round up = %u\n", lo);

	/* calculate real num of frames after set up core map */
	num_of_frames = (hi - lo)/PAGE_SIZE;
	total_frames = num_of_frames;
//	kprintf("total_frames = %d", total_frames);

	/* set up core map*/
	paddr_t temp_lo = lo;
	for (int i=0; i<num_of_frames; ++i) {
		core_map[i].start_addr = temp_lo;
		temp_lo += PAGE_SIZE;
		core_map[i].block_num = 1;
		core_map[i].total_block = 1;
		core_map[i].use = false;
	}
	#endif
}

static
paddr_t
getppages(unsigned long npages)
{
	paddr_t addr;
	/* find_block = true if we find a contiguous block */
	bool find_block = false;

	spinlock_acquire(&stealmem_lock);

	#if OPT_A3
	if (total_frames == 0) {
		addr = ram_stealmem(npages);
	} else {
		// i = index of core map, page suppose to = npages
		int i, page = 0; 
		for (i=0; i<total_frames; ++i) {
			if (!core_map[i].use) {
				++page;
				if (page == (int)npages) {
				find_block = true;
				break;
				}
			} else {
				page = 0;
			}
		} 
		if (find_block) {
			int count_block_number=1;
			for (int j=i-(int)npages+1; j<=i; ++j) {
				if (j == i-(int)npages+1) {
					addr = core_map[j].start_addr;
				}
				core_map[j].use = true;
				core_map[j].total_block = npages;
				core_map[j].block_num = count_block_number;
				++count_block_number;
			}
		} else {
			/* not enough contigues blocks for npages */
			spinlock_release(&stealmem_lock);
			return 0;
		}
	}

	#else 
	spinlock_acquire(&stealmem_lock);
	addr = ram_stealmem(npages);
	#endif

	spinlock_release(&stealmem_lock);
	return addr;
}

/* Allocate/free some kernel-space virtual pages */
vaddr_t 
alloc_kpages(int npages)
{
	paddr_t pa;
	pa = getppages(npages);
	if (pa==0) {
		return 0;
	}
	return PADDR_TO_KVADDR(pa);
}

void 
free_kpages(vaddr_t addr)
{
	#if OPT_A3
	spinlock_acquire(&stealmem_lock);
//	kprintf("vaddr = %u\n", addr);
	/* transfer vaddr to paddr */
	paddr_t paddr = addr - MIPS_KSEG0;
//	kprintf("paddr = %u\n", paddr);

	/* find the paddr in core map */
	for(int i=0; i<total_frames; ++i) {
		if (core_map[i].start_addr == paddr) {
			for (int j=0; j<=core_map[i].total_block; ++j) {
				core_map[i+j].use = false;
				core_map[i+j].total_block = 1;
				core_map[i+j].block_num = 1;
			}
			break;
		}
	}
	spinlock_release(&stealmem_lock);

	#else
	/* nothing - leak the memory. */

	(void)addr;
	#endif
}

void
vm_tlbshootdown_all(void)
{
	panic("dumbvm tried to do tlb shootdown?!\n");
}

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("dumbvm tried to do tlb shootdown?!\n");
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	vaddr_t vbase1, vtop1, vbase2, vtop2, stackbase, stacktop;
	paddr_t paddr;
	int i;
	uint32_t ehi, elo;
	struct addrspace *as;
	int spl;

	#if OPT_A3
	/* check if this entry is a text segment */
	bool text_seg = false;
	#endif

	faultaddress &= PAGE_FRAME;

	DEBUG(DB_VM, "dumbvm: fault: 0x%x\n", faultaddress);

	switch (faulttype) {
	    case VM_FAULT_READONLY:
		/* We always create pages read-write, so we can't get this */

		#if OPT_A3
	    return EINVAL;

		#else
		panic("dumbvm: got VM_FAULT_READONLY\n");
		#endif

	    case VM_FAULT_READ:
	    case VM_FAULT_WRITE:
		break;
	    default:
		return EINVAL;
	}

	if (curproc == NULL) {
		/*
		 * No process. This is probably a kernel fault early
		 * in boot. Return EFAULT so as to panic instead of
		 * getting into an infinite faulting loop.
		 */
		return EFAULT;
	}

	as = curproc_getas();
	if (as == NULL) {
		/*
		 * No address space set up. This is probably also a
		 * kernel fault early in boot.
		 */
		return EFAULT;
	}

	/* Assert that the address space has been set up properly. */
	KASSERT(as->as_vbase1 != 0);
	KASSERT(as->as_pbase1 != 0);
	KASSERT(as->as_npages1 != 0);
	KASSERT(as->as_vbase2 != 0);
	KASSERT(as->as_pbase2 != 0);
	KASSERT(as->as_npages2 != 0);
	KASSERT(as->as_stackpbase != 0);
	KASSERT((as->as_vbase1 & PAGE_FRAME) == as->as_vbase1);
	KASSERT((as->as_pbase1 & PAGE_FRAME) == as->as_pbase1);
	KASSERT((as->as_vbase2 & PAGE_FRAME) == as->as_vbase2);
	KASSERT((as->as_pbase2 & PAGE_FRAME) == as->as_pbase2);
	KASSERT((as->as_stackpbase & PAGE_FRAME) == as->as_stackpbase);

	#if OPT_A3
	KASSERT(as->as_pt1 != NULL);
	KASSERT(as->as_pt2 != NULL);
	KASSERT(as->as_stack_pt != NULL);
	#endif

	vbase1 = as->as_vbase1;
	vtop1 = vbase1 + as->as_npages1 * PAGE_SIZE;
	vbase2 = as->as_vbase2;
	vtop2 = vbase2 + as->as_npages2 * PAGE_SIZE;
	stackbase = USERSTACK - DUMBVM_STACKPAGES * PAGE_SIZE;
	stacktop = USERSTACK;

	if (faultaddress >= vbase1 && faultaddress < vtop1) {
		/* this is the text and code segment */
		paddr = (faultaddress - vbase1) + as->as_pbase1;
		#if OPT_A3
		text_seg = true;
		#endif
	}
	else if (faultaddress >= vbase2 && faultaddress < vtop2) {
		paddr = (faultaddress - vbase2) + as->as_pbase2;
	}
	else if (faultaddress >= stackbase && faultaddress < stacktop) {
		paddr = (faultaddress - stackbase) + as->as_stackpbase;
	}
	else {
		return EFAULT;
	}

	/* make sure it's page-aligned */
	KASSERT((paddr & PAGE_FRAME) == paddr);

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_read(&ehi, &elo, i);
		if (elo & TLBLO_VALID) {
			continue;
		}
		/* 
		valid bit != 1
		overwrites the unused entry with the virtual to physical address mapping
		required by the instruction that generated the TLB exception 
		*/ 
		ehi = faultaddress;
		elo = paddr | TLBLO_DIRTY | TLBLO_VALID;

		#if OPT_A3
		if (text_seg && as->complete_load_elf) {
			elo&=~TLBLO_DIRTY;
		}
		#endif

		DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
		tlb_write(ehi, elo, i);
		splx(spl);
		return 0;
	}

	#if OPT_A3
	/* call tlb_random to write the entry into a random TLB slot */
	ehi = faultaddress;
	elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
	if (text_seg && as->complete_load_elf) {
			elo&=~TLBLO_DIRTY;
	}
	tlb_random(ehi, elo);
//	kprintf("call tlb_random\n");
	splx(spl);
	return 0;

	#else
	kprintf("dumbvm: Ran out of TLB entries - cannot handle page fault\n");
	splx(spl);
	return EFAULT;
	#endif
}

struct addrspace *
as_create(void)
{
	struct addrspace *as = kmalloc(sizeof(struct addrspace));
	if (as==NULL) {
		return NULL;
	}

	#if OPT_A3
	as->as_vbase1 = 0;
	as->as_pt1 = NULL;
	as->as_pt1_executable = false;
	as->as_pt1_readable = false;
	as->as_pt1_writeable = false;
	as->as_npages1 = 0;

	as->as_vbase2 = 0;
	as->as_pt2 = NULL;	
	as->as_pt2_executable = false;
	as->as_pt2_readable = false;
	as->as_pt2_writeable = false;
	as->as_npages2 = 0;

	as->as_stack_pt = 0;

	as->complete_load_elf = false;
	#else
	as->as_vbase1 = 0;
	as->as_pbase1 = 0;
	as->as_npages1 = 0;
	as->as_vbase2 = 0;
	as->as_pbase2 = 0;
	as->as_npages2 = 0;
	as->as_stackpbase = 0;
	#endif

	return as;
}

void
as_destroy(struct addrspace *as)
{
	/* free pages in thest memory region */
	#if OPT_A3
	/*
	for (int i=0; i<total_frames; ++i) {
		core_map[i].use = false;
		core_map[i].total_block = 1;
		core_map[i].block_num = 1;
	}
	*/
	for (size_t i=0; i<as->as_npages1; ++i) {
		free_kpages(as->as_pt1[i]);
	}
	kfree(as->as_pt1);
	for (size_t i=0; i<as->as_npages2; ++i) {
		free_kpages(as->as_pt2[i]);
	}
	kfree(as->as_pt2);
	for (size_t i=0; i<DUMBVM_STACKPAGES; ++i) {
		free_kpages(as->as_stack_pt[i]);
	}
	kfree(as->as_stack_pt);
	#endif

	kfree(as);
}

void
as_activate(void)
{
	int i, spl;
	struct addrspace *as;

	as = curproc_getas();
#ifdef UW
        /* Kernel threads don't have an address spaces to activate */
#endif
	if (as == NULL) {
		return;
	}

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

void
as_deactivate(void)
{
	/* nothing */
}

int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
		 int readable, int writeable, int executable)
{
	size_t npages; 

	/* Align the region. First, the base... */
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = sz / PAGE_SIZE;

	#if OPT_A3
	/* Allocate (kmalloc) and initialize the page table for the specified segment */
	if (as->as_pt1 == NULL) {
		as->as_pt1 = kmalloc(npages * sizeof(struct PTE));
		as->as_pt1_readable = readable;
		as->as_pt1_writeable = writeable;
		as->as_pt1_executable = executable;
		as->as_vbase1 = vaddr;
		as->as_npages1 = npages;
		return 0;
	}
	if (as->as_pt2 == NULL) {
		as->as_pt2 = kmalloc(npages * sizeof(struct PTE));
		as->as_pt2_readable = readable;
		as->as_pt2_writeable = writeable;
		as->as_pt2_executable = executable;
		as->as_vbase2 = vaddr;
		as->as_npages2 = npages;
		return 0;
	}
	if (as->as_stack_pt == NULL) {
		as->as_stack_pt = kmalloc(DUMBVM_STACKPAGES * sizeof(struct PTE));
	}

	#else 
	
	/* We don't use these - all pages are read-write */
	(void)readable;
	(void)writeable;
	(void)executable;

//	#endif

	if (as->as_vbase1 == 0) {
		as->as_vbase1 = vaddr;
		as->as_npages1 = npages;
/*
		#if OPT_A3
		as->as_pbase1 = page_table;
		#endif
*/
		return 0;
	}

	if (as->as_vbase2 == 0) {
		as->as_vbase2 = vaddr;
		as->as_npages2 = npages;
/*
		#if OPT_A3
		as->as_pbase2 = page_table;
		#endif
*/
		return 0;
	}
	#endif
	/*
	 * Support for more than two regions is not available.
	 */
	kprintf("dumbvm: Warning: too many regions\n");
	return EUNIMP;
}

static
void
as_zero_region(paddr_t paddr, unsigned npages)
{
	bzero((void *)PADDR_TO_KVADDR(paddr), npages * PAGE_SIZE);
}

int
as_prepare_load(struct addrspace *as)
{
	#if OPT_A3
	as->as_pt1[0] = getppages(1);
	as->as_pbase1 = as->as_pt1[0];
	for (size_t i=1; i<as_npages1; ++i) {
		as->as_pt1[i] = getppages(1);
		if (as->as_pt1[i] == 0) return ENOMEM;
		as_zero_region(as->as_pt1[i], 1);
	}

	as->as_pt2[0] = getppages(1);
	as->as_pbase2 = as->as_pt2[0];
	for (size_t i=1; i<as_npages2; ++i) {
		as->as_pt2[i] = getppages(1);
		if (as->as_pt2[i] == 0) return ENOMEM;
		as_zero_region(as->as_pt2[i], 1);
	}

	as->as_stack_pt[0] = getppages(1);
	as->as_stackpbase = as->as_stack_pt[0];
	for (size_t i=1; i<DUMBVM_STACKPAGES; ++i) {
		as->as_stack_pt[i] = getppages(1);
		if (as->as_stack_pt[i] == 0) return ENOMEM;
		as_zero_region(as->as_stack_pt[i], 1);
	}

	#else 
	KASSERT(as->as_pbase1 == 0);
	KASSERT(as->as_pbase2 == 0);
	KASSERT(as->as_stackpbase == 0);

	as->as_pbase1 = getppages(as->as_npages1);
	if (as->as_pbase1 == 0) {
		return ENOMEM;
	}

	as->as_pbase2 = getppages(as->as_npages2);
	if (as->as_pbase2 == 0) {
		return ENOMEM;
	}

	as->as_stackpbase = getppages(DUMBVM_STACKPAGES);
	if (as->as_stackpbase == 0) {
		return ENOMEM;
	}
	
	as_zero_region(as->as_pbase1, as->as_npages1);
	as_zero_region(as->as_pbase2, as->as_npages2);
	as_zero_region(as->as_stackpbase, DUMBVM_STACKPAGES);
	#endif

	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	/* set the flag of completing load_elf to be true */
	#if OPT_A3
	as->complete_load_elf = true;
	#else

	(void)as;
	#endif

	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	KASSERT(as->as_stackpbase != 0);

	*stackptr = USERSTACK;
	return 0;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *new;

	new = as_create();
	if (new==NULL) {
		return ENOMEM;
	}

	new->as_vbase1 = old->as_vbase1;
	new->as_npages1 = old->as_npages1;
	new->as_vbase2 = old->as_vbase2;
	new->as_npages2 = old->as_npages2;

	#if OPT_A3
	new->as_pt1_readable = old->as_pt1_readable;
    new->as_pt1_writeable = old->as_pt1_writeable;
    new->as_pt1_executable = old->as_pt1_executable;
    
    new->as_pt2_readable   = old->as_pt2_readable;
    new->as_pt2_writeable  = old->as_pt2_writeable;
    new->as_pt2_executable = old->as_pt2_executable;
	#endif

	/* (Mis)use as_prepare_load to allocate some physical memory. */
	if (as_prepare_load(new)) {
		as_destroy(new);
		return ENOMEM;
	}

	KASSERT(new->as_pbase1 != 0);
	KASSERT(new->as_pbase2 != 0);
	KASSERT(new->as_stackpbase != 0);

	#if OPT_A3
	for (size_t i=0; i<new->as_npages1; ++i) {
		memmove((void *)PADDR_TO_KVADDR(new->as_pt1[i]),
                (const void *)PADDR_TO_KVADDR(old->as_pt1[i]),
                    old->as_npages1*PAGE_SIZE);
	}
	for (size_t i=0; i<new->as_npages2; ++i) {
		memmove((void *)PADDR_TO_KVADDR(new->as_pt2[i]),
                (const void *)PADDR_TO_KVADDR(old->as_pt2[i]),
                    old->as_npages2*PAGE_SIZE);
	}
	for (size_t i=0; i<DUMBVM_STACKPAGES; ++i) {
		memmove((void *)PADDR_TO_KVADDR(new->as_stack_pt[i]),
                (const void *)PADDR_TO_KVADDR(old->as_stack_pt[i]),
                    DUMBVM_STACKPAGES*PAGE_SIZE);
	}

	#else
	memmove((void *)PADDR_TO_KVADDR(new->as_pbase1),
		(const void *)PADDR_TO_KVADDR(old->as_pbase1),
		old->as_npages1*PAGE_SIZE);

	memmove((void *)PADDR_TO_KVADDR(new->as_pbase2),
		(const void *)PADDR_TO_KVADDR(old->as_pbase2),
		old->as_npages2*PAGE_SIZE);

	memmove((void *)PADDR_TO_KVADDR(new->as_stackpbase),
		(const void *)PADDR_TO_KVADDR(old->as_stackpbase),
		DUMBVM_STACKPAGES*PAGE_SIZE);
	
	*ret = new;
	return 0;
}
