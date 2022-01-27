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

//struct that will contain ref counts of each pa and locks
struct {
  struct spinlock lock;  
  int reference_counter[PHYSTOP/PGSIZE];
}ref_counter;


//---------------------------------------------------------------------------------------------

void                 //function that will set reference counter of pa=1
init_ref_counter(void *pa){     
  acquire(&ref_counter.lock);
  ref_counter.reference_counter[(uint64)pa/PGSIZE]=1;
  release(&ref_counter.lock);

}

void                 //function that will increase reference counter of pa by 1
increase_ref_counter(void *pa){   
    acquire(&ref_counter.lock);
  ref_counter.reference_counter[(uint64)pa/PGSIZE]++;
    release(&ref_counter.lock);

}

void                 //function that will decrease reference counter of pa by 1 
decrease_ref_counter(void *pa){
    acquire(&ref_counter.lock);
  --ref_counter.reference_counter[(uint64)pa/PGSIZE];
  release(&ref_counter.lock);
}

int       //function that re
return_ref_counter(void *pa){
  int a;
  a = ref_counter.reference_counter[(uint64)pa/PGSIZE];
  return a;
}

//------------------------------------------------------------------------

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&ref_counter.lock, "ref_counter");   //init ref lock
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    init_ref_counter((void*)p); //initialiaze reference counter of p=1
    kfree(p);   //then free p
  }
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  if((return_ref_counter(pa)-1)<=0){   //if ref counter-1=0 then there are no processes linked to this pa so it will be freed  
    memset(pa, 1, PGSIZE);

    r = (struct run*)pa;
    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);

  }
  else{    //if ref counter of pa>2 then just decrease its ref counter 
    decrease_ref_counter(pa);   
  }
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
  if(r){
    kmem.freelist = r->next;
    init_ref_counter((void*)r);  // initialise the ref counter of the new pa to 1
  }  
  release(&kmem.lock);
  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

