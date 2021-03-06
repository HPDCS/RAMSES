#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <core.h>
#include <numerical.h>
#include "dymelor.h"

/// Recoverable memory state for LPs
malloc_state **recoverable_state;

void recoverable_init(void) {

	register unsigned int i;

	recoverable_state = __real_malloc(sizeof(malloc_state *) * region_c);

	for (i = 0; i < region_c; i++) {

		recoverable_state[i] = __real_malloc(sizeof(malloc_state));
		if (recoverable_state[i] == NULL)
			rootsim_error(true, "Unable to allocate memory on malloc init");

		malloc_state_init(true, recoverable_state[i]);
	}
}

void recoverable_fini(void) {
	unsigned int i, j;
	malloc_area *current_area;

	for (i = 0; i < region_c; i++) {
		for (j = 0; j < (unsigned int)recoverable_state[i]->num_areas; j++) {
			current_area = &(recoverable_state[i]->areas[j]);
			if (current_area != NULL) {
				if (current_area->self_pointer != NULL) {
#ifdef HAVE_PARALLEL_ALLOCATOR
					pool_release_memory(i, current_area->self_pointer);
#else
					__real_free(current_area->self_pointer);
#endif
				}
			}
		}
		__real_free(recoverable_state[i]->areas);
		__real_free(recoverable_state[i]);
	}
	__real_free(recoverable_state);
}

/**
* This function returns the whole size of a state. It can be used as the total size to pack a log
*
* @author Alessandro Pellegrini
* @author Roberto Vitali
*
* @param log The pointer to the log, or to the state
* @return The whole size of the state (metadata included)
*
*/
size_t get_log_size(malloc_state * logged_state) {
	if (logged_state == NULL)
		return 0;

	if (is_incremental(logged_state)) {
		return sizeof(malloc_state) + sizeof(seed_type) + logged_state->dirty_areas * sizeof(malloc_area) + logged_state->dirty_bitmap_size + logged_state->total_inc_size;
	} else {
		return sizeof(malloc_state) + sizeof(seed_type) + logged_state->busy_areas * sizeof(malloc_area) + logged_state->bitmap_size + logged_state->total_log_size;
	}
}

/**
* This is the wrapper of the real stdlib malloc(). Whenever the application level software
* calls malloc, the call is redirected to this piece of code which uses the memory preallocated
* by the DyMeLoR subsystem for serving the request. If the memory in the malloc_area is exhausted,
* a new one is created, relying on the stdlib malloc.
* In future releases, this wrapper will be integrated with the Memory Management subsystem,
* which is not yet ready for production.
*
* @author Roberto Toccaceli
* @author Francesco Quaglia
*
* @param size Size of the allocation
* @return A pointer to the allocated memory
*
*/
void *__wrap_malloc(size_t size) {
	void *ptr;

	ptr = do_malloc(current_lp, recoverable_state[current_lp], size);

	return ptr;
}

/**
* This is the wrapper of the real stdlib free(). Whenever the application level software
* calls free, the call is redirected to this piece of code which will set the chunk in the
* corresponding malloc_area as not allocated.
*
* For further information, please see the paper:
* 	R. Toccaceli, F. Quaglia
* 	DyMeLoR: Dynamic Memory Logger and Restorer Library for Optimistic Simulation Objects
* 	with Generic Memory Layout
*	Proceedings of the 22nd Workshop on Principles of Advanced and Distributed Simulation
*	2008
*
* @author Roberto Toccaceli
* @author Francesco Quaglia
*
* @param ptr A memory buffer to be free'd
*
*/
void __wrap_free(void *ptr) {
	do_free(current_lp, recoverable_state[current_lp], ptr);
}

/**
* This is the wrapper of the real stdlib realloc(). Whenever the application level software
* calls realloc, the call is redirected to this piece of code which rely on wrap_malloc
*
* @author Roberto Vitali
*
* @param ptr The pointer to be buffer to be reallocated
* @param size The size of the allocation
* @return A pointer to the newly allocated buffer
*
*/
void *__wrap_realloc(void *ptr, size_t size) {

	void *new_buffer;
	size_t old_size;
	malloc_area *m_area;

	// If ptr is NULL realloc is equivalent to the malloc
	if (ptr == NULL) {
		return __wrap_malloc(size);
	}
	// If ptr is not NULL and the size is 0 realloc is equivalent to the free
	if (size == 0) {
		__wrap_free(ptr);
		return NULL;
	}

	m_area = get_area(ptr);

	// The size could be greater than the real request, but it does not matter since the realloc specific requires that
	// is copied at least the smaller buffer size between the new and the old one
	old_size = m_area->chunk_size;

	new_buffer = __wrap_malloc(size);

	if (new_buffer == NULL)
		return NULL;

	memcpy(new_buffer, ptr, size > old_size ? size : old_size);
	__wrap_free(ptr);

	return new_buffer;
}

/**
* This is the wrapper of the real stdlib calloc(). Whenever the application level software
* calls calloc, the call is redirected to this piece of code which relies on wrap_malloc
*
* @author Roberto Vitali
*
* @param size The size of the allocation
* @return A pointer to the newly allocated buffer
*
*/
void *__wrap_calloc(size_t nmemb, size_t size) {

	void *buffer;

	if (nmemb == 0 || size == 0)
		return NULL;

	buffer = __wrap_malloc(nmemb * size);
	if (buffer == NULL)
		return NULL;

	bzero(buffer, nmemb * size);

	return buffer;
}

void clean_buffers_on_gvt(unsigned int lid, simtime_t time_barrier) {

	int i;
	malloc_state *state;
	malloc_area *m_area;

	state = recoverable_state[lid];

	// The first NUM_AREAS malloc_areas are placed according to their chunks' sizes. The exceeding malloc_areas can be compacted
	for (i = NUM_AREAS; i < state->num_areas; i++) {
		m_area = &state->areas[i];

		if (m_area->alloc_chunks == 0 && m_area->last_access < time_barrier && !CHECK_AREA_LOCK_BIT(m_area)) {

			if (m_area->self_pointer != NULL) {

#ifdef HAVE_PARALLEL_ALLOCATOR
				pool_release_memory(lid, m_area->self_pointer);
#else
				__real_free(m_area->self_pointer);
#endif

				m_area->use_bitmap = NULL;
				m_area->dirty_bitmap = NULL;
				m_area->self_pointer = NULL;
				m_area->area = NULL;
				m_area->state_changed = 0;

				// Set the pointers
				if (m_area->prev != -1)
					state->areas[m_area->prev].next = m_area->next;
				if (m_area->next != -1)
					state->areas[m_area->next].prev = m_area->prev;

				// Swap, if possible
				if (i < state->num_areas - 1) {
					memcpy(m_area, &state->areas[state->num_areas - 1], sizeof(malloc_area));
					m_area->idx = i;
					if (m_area->prev != -1)
						state->areas[m_area->prev].next = m_area->idx;
					if (m_area->next != -1)
						state->areas[m_area->next].prev = m_area->idx;

					// Update the self pointer
					*(long long *)m_area->self_pointer = (long long)m_area;

					// The swapped area will now be checked
					i--;
				}
				// Decrement the expected number of areas
				state->num_areas--;
			}
		}
	}
}
