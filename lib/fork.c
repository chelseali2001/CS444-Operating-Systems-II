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
  
  // Check that the faulting access was a write
	if (!(err & FEC_WR))
		panic("faulting access was not a write");

  // Check that the faulting access was to a copy-on-write page.
  if (!(uvpt[PGNUM(addr)] & PTE_COW))
    panic("faulting access is not a copy-on-write page");

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
 
  // allocate a page at the address PFTEMP
	if ((r = sys_page_alloc(0, PFTEMP, PTE_U | PTE_P | PTE_W)) < 0)
		panic("sys_page_alloc: %e", r);
   
  // Copy the data from the old page to the new page
	memcpy(PFTEMP, (void *) PTE_ADDR(addr), PGSIZE);
 
  // Move the new page to the old page's address.
	if ((r = sys_page_map(0, PFTEMP, 0, (void *) PTE_ADDR(addr), PTE_U | PTE_P | PTE_W)) < 0)
		panic("sys_page_map: %e", r);

	if ((r = sys_page_unmap(0, PFTEMP)) < 0)
		panic("sys_page_unmap: %e", r);
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
  
  // If the page is writable or copy-on-write, the new mapping must be created copy-on-write, and then our mapping must be marked copy-on-write as well
	if ((uvpt[pn] & PTE_W) || (uvpt[pn] & PTE_COW)) {
    // Changes the permission of both pages as PTE_P | PTW_U | PTW_COW
    // Mapping for child
		if ((r = sys_page_map(0, (void *) (pn * PGSIZE), envid, (void *) (pn * PGSIZE), PTE_P | PTE_U | PTE_COW)) < 0)
			panic("sys_page_map: %e", r);

    // Mapping for parent
		if ((r = sys_page_map(0, (void *) (pn * PGSIZE), 0, (void *) (pn * PGSIZE), PTE_P | PTE_U | PTE_COW)) < 0)
			panic("sys_page_map: %e", r);
	} else {
    // Mapping pages that are present but not writable or copy-on-write
    if ((r = sys_page_map(0, (void *) (pn * PGSIZE), envid, (void *) (pn * PGSIZE), PTE_U|PTE_P)) < 0)
      panic("sys_page_map: %e", r);
  }

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
  envid_t envid;
  uint32_t addr;
  int r;
  
  // Set page fault handler
  set_pgfault_handler(&pgfault);
  
  // Create a child
  envid = sys_exofork();
  
  // Return < 0 on error
  if (envid < 0)
    panic("sys_exofork: %e", envid);
  
  // Return 0 to the child
  if (envid == 0) {
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
  } 
  
  // Copy our address space and page fault handler setup to the child.
  for (addr = 0; addr < USTACKTOP; addr += PGSIZE)
		if ((uvpd[PDX(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_U))
			duppage(envid, PGNUM(addr));
  
	extern void _pgfault_upcall();

  // Allocate a new page at UXSTACKTOP � PGSIZE
	if ((r = sys_page_alloc(envid, (void *)(UXSTACKTOP - PGSIZE), PTE_P | PTE_U | PTE_W)) < 0)
		panic("sys_page_alloc: %e", r);
 
  // Child must have set its page fault handler to handle CoW
  if ((r = sys_env_set_pgfault_upcall(envid, _pgfault_upcall)) < 0)
    panic("sys_env_set_pgfault_upcall: %e", r);

  // Make child runnable after finishing the Copy-on-Write fork!
	if ((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0)
		panic("sys_env_set_status: %e", r);
   
  return envid; // Return child's envid to the parent
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
