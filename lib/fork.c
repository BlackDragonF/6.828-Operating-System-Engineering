// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
    // use uvpt to get PTE and check premissions
    if (!(err & FEC_WR) ||
        !(((pte_t *)uvpt)[PGNUM(addr)] & PTE_COW)) {
        panic("fault isn't write or PTE is not marked as PTE_COW or both!");
    }

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
    // allocate a page and map it at PFTEMP
    // use sys_page_alloc system call
    // NB: use 0 instead of thisenv->env_id
    // for normally when child environment scheduled by JOS
    // first it tries to store 0(return value of fork()) to
    // local variable child_env_id on the stack
    // which will take a page fault and kernel will trasfer control
    // to pgfault here
    // thisenv->env_id IS ABSOLUTELY WRONG here, so use 0
    // and let kernel convert from 0 to correct env_id
    if ((r = sys_page_alloc(0, (void *)PFTEMP,
        PTE_P | PTE_U | PTE_W)) < 0) {
        panic("failed to allocate page at temp location - %e, %x!", r, thisenv->env_id);
    }

    addr = ROUNDDOWN(addr, PGSIZE);
    // use memcpy to copy from fault address's page to PFTEMP's newly
    // allocated page
    memcpy((void *)PFTEMP, addr, PGSIZE);

    // use sys_page_map system call to map newly allocated
    // page at PFTEMP at addr with read/write permissions
    if ((r = sys_page_map(0, (void *)PFTEMP,
                          0, addr,
                          PTE_P | PTE_U | PTE_W)) < 0) {
        panic("failed to map new page - %e!", r);
    }
    // use sys_page_unmap system call to unmap page at
    // temp location PFTEMP
    if ((r = sys_page_unmap(0, (void *)PFTEMP)) < 0) {
        panic("failed to unmap page at PFTEMP - %e!", r);
    }

	// panic("pgfault not implemented");
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
    int perm = PTE_P | PTE_U;
    if ((((pte_t *)uvpt)[pn] & (PTE_W | PTE_COW))) {
        // check for writable or copy-on-write page
        // and add perm with PTE_COW
        perm |= PTE_COW;
        // NB: we also use 0 to replace thisenv->env_id
        // for in unix-like fork() implementation, we will copy
        // address space from parent process to child process
        // at current time, thisenv->env_id IS ABSOLUTELY WRONG

        // map from parent environment to child environment
        if (sys_page_map(0,     (void *)(pn * PGSIZE),
                         envid, (void *)(pn * PGSIZE), perm) < 0) {
            panic("failed to map page from parent to child!");
        }
        // remap parent's environment
        if (sys_page_map(0, (void *)(pn * PGSIZE),
                         0, (void *)(pn * PGSIZE), perm) < 0) {
            panic ("failed to remap page in parent!");
        }
    } else {
        // for other pages, simply map from
        // parent environment to child environment
        if (sys_page_map(0,     (void *)(pn * PGSIZE),
                         envid, (void *)(pn * PGSIZE), perm) < 0) {
            panic("failed to map page from parent to child!");
        }
    }

	// panic("duppage not implemented");

	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
    int r;

    // use set_pgfault_handler to install pgfault() as page fault handler
    set_pgfault_handler(pgfault);

    // use system call to create new blank child environment
    envid_t child_env_id = sys_exofork();
    if (child_env_id <= 0) {
        // error when create new environment, simply return
        return child_env_id;
    }

    // traverse through parent's address space and use duppage
    // to copy address mappings
    uintptr_t addr;
    for (addr = 0 ; addr < UTOP ; addr += PGSIZE) {
        if ((((pde_t *)uvpd)[PDX(addr)] & PTE_P) &&
            (((pte_t *)uvpt)[PGNUM(addr)] & PTE_P)) {
            // if both page directory entry and page table entry
            // exist for one address(page-aligned), then call duppage
            if (addr == (UXSTACKTOP - PGSIZE)) {
                // ignore user exception stack
                // for we will map a page for user exception later
                continue;
            }
            if ((r = duppage(child_env_id, PGNUM(addr))) < 0) {
                // duppage failed
                // destroy child environment and return
                goto error_handle;
            }
        }
    }

    // child must have its own exception stack
    // so alloc page and map it for child here
    if ((r = sys_page_alloc(child_env_id, (void *)(UXSTACKTOP - PGSIZE),
                            PTE_P | PTE_U | PTE_W)) < 0) {
        // failed to set child environment's exception stack
        // destroy child environment and return
        goto error_handle;
    }

    // set child's page fault handler so that parent
    // and child looks the same
    // use sys_env_set_pgfault_upcall system call
    extern void _pgfault_upcall(void);
    if ((r = sys_env_set_pgfault_upcall(child_env_id, _pgfault_upcall)) < 0) {
        // failed to set child's page fault upcall
        // destroy child environment and return
        goto error_handle;
    }

    // now child is ready to run, set child to ENV_RUNNABLE
    // use sys_env_set_status system call
    if ((r = sys_env_set_status(child_env_id, ENV_RUNNABLE)) < 0) {
        // failed to change child environment's running status
        // destroy child environment and return
        goto error_handle;
    }

	// panic("fork not implemented");

    return child_env_id;

// use goto to avoid meaningless error_handle code
// copy and paste everywhere
error_handle:
    if (sys_env_destroy(child_env_id) < 0) {
        // failed either, panic then
        panic("failed to destroy child environment!");
    }
    return r;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
