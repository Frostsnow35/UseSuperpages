// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

// Reserve 32 MiB for 2 MiB pages; ordinary kalloc never uses this range.
#define NSUPER 16
#define SUPERBASE (PHYSTOP - NSUPER * SUPERPGSIZE)
#define SUPERPAGES (SUPERPGSIZE / PGSIZE)

struct {
  struct spinlock lock;
  struct run *freelist;
  struct {
    // -1: one superpage, 0: free, >0: live 4 KiB pages after demotion.
    int refs;
    pagetable_t split;
  } block[NSUPER];
} supermem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&supermem.lock, "supermem");
  if((uint64)end > SUPERBASE)
    panic("kinit: superpage reserve");
  freerange(end, (void*)SUPERBASE);
  for(int i = 0; i < NSUPER; i++){
    supermem.block[i].refs = -1;
    superfree((void*)(SUPERBASE + i * SUPERPGSIZE));
  }
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Demoted pages stay in their original 2 MiB block. Recycle the whole
  // block only after all 512 subpages have been released.
  if((uint64)pa >= SUPERBASE){
    int i = ((uint64)pa - SUPERBASE) / SUPERPGSIZE;
    acquire(&supermem.lock);
    if(supermem.block[i].refs <= 0)
      panic("kfree: unsplit superpage");
    memset(pa, 1, PGSIZE);
    if(--supermem.block[i].refs == 0){
      r = (struct run*)(SUPERBASE + i * SUPERPGSIZE);
      r->next = supermem.freelist;
      supermem.freelist = r;
    }
    release(&supermem.lock);
    return;
  }

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

// Allocate one aligned 2 MiB block and reserve its future L0 page table.
// Reserving the table now makes shrinking a superpage possible at OOM.
void *
superalloc(void)
{
  struct run *r;
  int i;

  acquire(&supermem.lock);
  r = supermem.freelist;
  if(r){
    supermem.freelist = r->next;
    i = ((uint64)r - SUPERBASE) / SUPERPGSIZE;
    supermem.block[i].refs = -1;
  }
  release(&supermem.lock);
  if(r == 0)
    return 0;

  pagetable_t split = (pagetable_t)kalloc();
  if(split == 0){
    superfree(r);
    return 0;
  }
  supermem.block[i].split = split;
  memset(r, 5, SUPERPGSIZE);
  return r;
}

// Free an intact superpage. Demoted pages instead go through kfree().
void
superfree(void *pa)
{
  uint64 addr = (uint64)pa;
  if(addr < SUPERBASE || addr >= PHYSTOP || addr % SUPERPGSIZE)
    panic("superfree");
  int i = (addr - SUPERBASE) / SUPERPGSIZE;

  acquire(&supermem.lock);
  if(supermem.block[i].refs != -1)
    panic("superfree: not intact");
  pagetable_t split = supermem.block[i].split;
  supermem.block[i].split = 0;
  supermem.block[i].refs = 0;
  memset(pa, 1, SUPERPGSIZE);
  struct run *r = (struct run*)pa;
  r->next = supermem.freelist;
  supermem.freelist = r;
  release(&supermem.lock);
  if(split)
    kfree(split);
}

// Transfer the reserved page table to vm.c and start tracking subpages.
// No data is moved: the 512 mappings still refer to this physical block.
pagetable_t
superdemote(void *pa)
{
  uint64 addr = (uint64)pa;
  if(addr < SUPERBASE || addr >= PHYSTOP || addr % SUPERPGSIZE)
    panic("superdemote");
  int i = (addr - SUPERBASE) / SUPERPGSIZE;

  acquire(&supermem.lock);
  pagetable_t split = supermem.block[i].split;
  if(supermem.block[i].refs != -1 || split == 0)
    panic("superdemote: not intact");
  supermem.block[i].split = 0;
  supermem.block[i].refs = SUPERPAGES;
  release(&supermem.lock);
  return split;
}
