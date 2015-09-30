
/* Struttura dati "Coda con priorità" per la schedulazione degli eventi (provissoria):
 * Estrazione a costo O(n)
 * Dimensione massima impostata a tempo di compilazione
 * Estrazione Thread-safe (non lock-free)
 * Inserimenti avvengono in transazioni
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "message.h"
#include "calqueue.h"
#include "core.h"


typedef struct __event_pool_node {
	msg_t message;
	calqueue_node *calqueue_node_reference;

} event_pool_node;

typedef struct __queue_t {
	event_pool_node *head;
	unsigned int size;

} queue_t;

queue_t _queue;

typedef struct __temp_thread_pool {
	unsigned int _thr_pool_count;
	msg_t messages[THR_POOL_SIZE] __attribute__ ((aligned(64)));
	simtime_t min_time;
} __temp_thread_pool;

__thread __temp_thread_pool _thr_pool __attribute__ ((aligned(64)));

int queue_lock = 0;

void reset_outgoing_msg(void) {
	_thr_pool._thr_pool_count = 0;
}

void queue_init(void) {

	calqueue_init();
}

void insert_message(unsigned int receiver, unsigned int entity1, unsigned int entity2, interaction_f interaction, update_f update, simtime_t timestamp, unsigned int event_type, void *event_content, unsigned int event_size) {
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

msg_t *get_min_timestamp_event(void) {
	return calqueue_get();
}

double flush_messages(void) {
	msg_t *new_hole;
	unsigned int i;
	double mintime;

	for (i = 0; i < _thr_pool._thr_pool_count; i++) {
		// calqueue_insert makes a copy of the buffer
		new_hole = &_thr_pool.messages[i];
		calqueue_insert(new_hole->timestamp, new_hole, sizeof(msg_t));
	}

	if (i == 0) {
		mintime = -1;
	} else {
		mintime = _thr_pool.min_time;
	}

	_thr_pool._thr_pool_count = 0;
	_thr_pool.min_time = INFTY;

	return mintime;
}
