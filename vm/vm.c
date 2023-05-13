/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "threads/mmu.h"
#include "vm/vm.h"
#include "vm/inspect.h"

unsigned page_hash(const struct hash_elem *p_, void *aux UNUSED);
bool page_less(const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);
void page_initialized(struct page *init_page, void *upage, vm_initializer *init, enum vm_type type, void *aux);
bool get_initializer_type(enum vm_type type);
/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */

void vm_init(void)
{
	vm_anon_init();
	vm_file_init();
#ifdef EFILESYS /* For project 4 */
	pagecache_init();
#endif
	register_inspect_intr();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type(struct page *page)
{
	int ty = VM_TYPE(page->operations->type);
	switch (ty)
	{
	case VM_UNINIT:
		return VM_TYPE(page->uninit.type);
	default:
		return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim(void);
static bool vm_do_claim_page(struct page *page);
static struct frame *vm_evict_frame(void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */


bool vm_alloc_page_with_initializer(enum vm_type type, void *upage, bool writable,
									vm_initializer *init, void *aux)
{

	ASSERT(VM_TYPE(type) != VM_UNINIT)
	struct page *init_page;
	struct supplemental_page_table *spt = &thread_current()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page(spt, upage) == NULL)
	{
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		
		init_page = (struct page *)malloc(sizeof(struct page));
		page_initialized(init_page, upage, init, type, aux);
		init_page->writable = writable;
		/* TODO: Insert the page into the spt. */
		return spt_insert_page(spt, init_page);
	}
err:
	return false;
}

void 
page_initialized(struct page *init_page, void *upage, vm_initializer *init, enum vm_type type, void *aux)
{
	if (VM_TYPE(type) == VM_ANON)
	{
		uninit_new(init_page, upage, init, type, aux, anon_initializer);
		return;
	}
	uninit_new(init_page, upage, init, type, aux, file_backed_initializer);
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page(struct supplemental_page_table *spt UNUSED, void *va UNUSED)
{
	struct page *page = malloc(sizeof(struct page));
	struct hash_elem *hash_element;
	/* TODO: Fill this function. */
	page->va = pg_round_down(va);
	hash_element = hash_find(&spt->spt_hash, &page->hash_elem);
	if (hash_element == NULL)
	{
		return NULL;
	}
	return hash_entry(hash_element, struct page, hash_elem);
}

/* Insert PAGE into spt with validation. */
bool spt_insert_page(struct supplemental_page_table *spt UNUSED,
					 struct page *page UNUSED)
{
	bool succ = false;
	/* TODO: Fill this function. */
	if (hash_insert(&spt->spt_hash, &page->hash_elem) == NULL)
	{
		succ = true;
	}

	return succ;
}

void spt_remove_page(struct supplemental_page_table *spt, struct page *page)
{
	vm_dealloc_page(page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim(void)
{
	struct frame *victim = NULL;
	/* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame(void)
{
	struct frame *victim UNUSED = vm_get_victim();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame(void)
{
	struct frame *frame = malloc(sizeof(struct frame));
	/* TODO: Fill this function. */
	frame->kva = palloc_get_page(PAL_USER);
	frame->page = NULL;
	ASSERT(frame != NULL);
	ASSERT(frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth(void *addr UNUSED)
{
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp(struct page *page UNUSED)
{
}

/* Return true on success */
bool vm_try_handle_fault(struct intr_frame *f UNUSED, void *addr UNUSED,
						 bool user UNUSED, bool write UNUSED, bool not_present UNUSED)
{
	struct thread* current_thread = thread_current();
	struct supplemental_page_table *spt UNUSED = &current_thread->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	if (not_present)
	{
		page = spt_find_page(spt, addr);
		if (page == NULL)
			return false;
		if (write == 1 && page->writable == 0) // write 불가능한 페이지에 write 요청한 경우
			return false;
		return vm_do_claim_page(page);
	}
	printf("try_handle_fault : return false\n");
	/* TODO: Your code goes here */
	return false;
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void vm_dealloc_page(struct page *page)
{
	destroy(page);
	free(page);
}

/* Claim the page that allocate on VA. */
bool vm_claim_page(void *va UNUSED)
{
	struct thread *current_thread = thread_current();
	struct page *page = spt_find_page(&(current_thread->spt), va);
	/* TODO: Fill this function */
	if (page == NULL)
	{
		PANIC("TODO");
	}

	return vm_do_claim_page(page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page(struct page *page)
{
	struct frame *frame = vm_get_frame();
	struct thread *current_thread = thread_current();
	/* Set links */
	frame->page = page;
	page->frame = frame;
	bool writable = page->writable;
	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	pml4_set_page(current_thread->pml4, page->va, frame->kva, writable);

	return swap_in(page, frame->kva);
}

/* Initialize new supplemental page table */
void supplemental_page_table_init(struct supplemental_page_table *spt UNUSED)
{
	hash_init(&(spt->spt_hash), page_hash, page_less, NULL);
}

/* Copy supplemental page table from src to dst */
bool supplemental_page_table_copy(struct supplemental_page_table *dst UNUSED,
								  struct supplemental_page_table *src UNUSED)
{
	struct hash_iterator hash_ite;
	hash_first(&hash_ite, src);
	while(hash_next(&hash_ite)){
		struct page *src_page = hash_entry(hash_cur(&hash_ite), struct page, hash_elem);
		struct page *dst_page = malloc(sizeof(struct page));
		memcpy(dst_page, src_page, sizeof(struct page));
		if(VM_TYPE(dst_page->operations->type) != VM_UNINIT){
			vm_do_claim_page(dst_page);
			memcpy(dst_page->va, src_page->frame->kva, PGSIZE);
		}
		spt_insert_page(dst, dst_page);	
		
	}
	return true;
}

/* Free the resource hold by the supplemental page table */
void supplemental_page_table_kill(struct supplemental_page_table *spt UNUSED)
{
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	struct hash_iterator hash_ite;
	hash_first(&hash_ite, spt);
	while(hash_next(&hash_ite)){
		struct page *src_page = hash_entry(hash_cur(&hash_ite), struct page, hash_elem);
		destroy(src_page);
	}
}
/* Returns a hash value for page p. */
unsigned
page_hash(const struct hash_elem *p_, void *aux UNUSED)
{
	const struct page *page = hash_entry(p_, struct page, hash_elem);
	return hash_bytes(&page->va, sizeof page->va);
}

/* Returns true if page a precedes page b. */
bool page_less(const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED)
{
	const struct page *a = hash_entry(a_, struct page, hash_elem);
	const struct page *b = hash_entry(b_, struct page, hash_elem);

	return a->va < b->va;
}
