
/* Struttura dati "Coda con priorità" per la schedulazione degli eventi (provissoria):
 * Estrazione a costo O(n)
 * Dimensione massima impostata a tempo di compilazione
 * Estrazione Thread-safe (non lock-free)
 * Inserimenti avvengono in transazioni
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "queue.h"
#include "message_state.h"
#include "calqueue.h"
#include "core.h"


extern simtime_t *current_time_vector;

typedef struct __event_pool_node {
	msg_t message;
	calqueue_node *calqueue_node_reference;

} event_pool_node;

typedef struct __queue_t {
	event_pool_node *head;
	unsigned int size;

} queue_t;

/*
typedef struct __temp_thread_pool
{
  msg_t *_tmp_mem __attribute__ ((aligned (64)));
  void *_tmp_msg_data;
  void *curr_msg_data;
  simtime_t min_time;
  unsigned int non_commit_size;
  
} temp_thread_pool;
*/

queue_t _queue;

typedef struct __temp_thread_pool {
	unsigned int _thr_pool_count;
	msg_t messages[THR_POOL_SIZE] __attribute__ ((aligned(64)));
	simtime_t min_time;
} __temp_thread_pool;

__thread __temp_thread_pool _thr_pool __attribute__ ((aligned(64)));



int queue_lock = 0;
int *region_lock;


void reset_outgoing_msg(void) {
	_thr_pool._thr_pool_count = 0;
}

void queue_init(void) {
	
	calqueue_init();
}

void queue_insert(unsigned int receiver, unsigned int entity1, unsigned int entity2, interaction_f interaction, update_f update, simtime_t timestamp, unsigned int event_type, void *event_content, unsigned int event_size) {
	msg_t *msg_ptr;

	if (_thr_pool._thr_pool_count == THR_POOL_SIZE) {
		printf("queue overflow: inserting event %d over %d\n", _thr_pool._thr_pool_count, THR_POOL_SIZE);
		printf("-----------------------------------------------------------------\n");
		abort();
	}

	if (event_size > MAX_DATA_SIZE) {
		printf("Error: requested a message of size %d, max allowed is %d\n", event_size, MAX_DATA_SIZE);
		abort();
	}
//  printf("QUEUE: receiver: %u, entity1: %u, entity2: %u, interaction: %p, update: %p, timestamp: %f, type: %d, content: %p, size: %u\n", receiver, entity1, entity2, interaction, update, timestamp, event_type, event_content, event_size);

	if (timestamp < _thr_pool.min_time)
		_thr_pool.min_time = timestamp;

	msg_ptr = &_thr_pool.messages[_thr_pool._thr_pool_count++];

	msg_ptr->sender_id = current_lp;
	msg_ptr->receiver_id = receiver;
	msg_ptr->timestamp = timestamp;
	msg_ptr->data_size = event_size;
	msg_ptr->type = event_type;
	msg_ptr->entity1 = entity1;
	msg_ptr->entity2 = entity2;
	msg_ptr->interaction = interaction;
	msg_ptr->update = update;

	memcpy(msg_ptr->data, event_content, event_size);
}

double queue_pre_min(void) {
	return _thr_pool.min_time;
}

double queue_deliver_msgs(void) {
	msg_t *new_hole;
	unsigned int i;
	double mintime;

	for (i = 0; i < _thr_pool._thr_pool_count; i++) {
		new_hole = malloc(sizeof(msg_t));
		memcpy(new_hole, &_thr_pool.messages[i], sizeof(msg_t));
		calqueue_put(new_hole->timestamp, new_hole);
	}

	if(i == 0) {
		mintime = -1;
	} else {
		mintime = _thr_pool.min_time;
	}

	_thr_pool._thr_pool_count = 0;
	_thr_pool.min_time = INFTY;

	return mintime;
}

msg_t *queue_min(void) {
	//event_pool_node *node_ret;
	msg_t *node_ret = NULL;

	// Gets the minimum timestamp event from the queue
	while (__sync_lock_test_and_set(&queue_lock, 1))
		while (queue_lock) ;

	node_ret = calqueue_get();
	if (node_ret == NULL) {
		current_time_vector[tid] = INFTY;
		__sync_lock_release(&queue_lock);
		return NULL;
	}

	current_time_vector[tid] = node_ret->timestamp;

	__sync_lock_release(&queue_lock);

	log_info(NC, "Get event with time %f\n", current_msg.timestamp);

	execution_time(node_ret->timestamp);

	return node_ret;
}

void queue_destroy(void) {
	free(_queue.head);
}
