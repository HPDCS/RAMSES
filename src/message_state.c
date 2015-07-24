#include <stdlib.h>
#include <limits.h>

#include <queue.h>
#include "core.h"
#include "message_state.h"

simtime_t *current_time_vector;
//static simtime_t *outgoing_time_vector;
static simtime_t *waiting_time_vector;
static simtime_t *waiting_time_who;
int *waiting_time_lock;

extern int queue_lock;

void message_state_init(void) {
	unsigned int i;

	current_time_vector = malloc(sizeof(simtime_t) * n_cores);
	waiting_time_vector = malloc(sizeof(simtime_t) * region_c);
	waiting_time_who = malloc(sizeof(simtime_t) * region_c);
	waiting_time_lock = malloc(sizeof(int) * region_c);
	region_lock = malloc(sizeof(int) * region_c);

	for (i = 0; i < n_cores; i++) {
		current_time_vector[i] = INFTY;
	}

	for (i = 0; i < region_c; i++) {
		waiting_time_vector[i] = INFTY;
	}
}

void execution_time(msg_t * msg) {
	unsigned int region;
	simtime_t time;
	simtime_t waiting_time;

	time = msg->timestamp;

	// Gets the lock on the region
	region = msg->receiver_id;

	log_info(NC, "Event with time %f tring to acquired lock on region %d\n", time, region);

 retry:
	if (__sync_lock_test_and_set(&region_lock[region], 1) == 1) {
		// If the thread cannot acquire the lock, this means that another
		// one is performing an event on that region; it has to register
		// its event's timestamp on the waiting queue, if and only if that
		// time is less then the possible already registerd one.

		while (__sync_lock_test_and_set(&waiting_time_lock[region], 1))
			while (waiting_time_lock[region]) ;

		waiting_time = waiting_time_vector[region];

		if (time < waiting_time) {

			waiting_time_vector[region] = time;
			waiting_time_who[region] = tid;
		}

		__sync_lock_release(&waiting_time_lock[region]);

		// Spins over the region_lock until the current event time is grater
		// than the waiting one

		while ((time > waiting_time) || (time == waiting_time && waiting_time_who[region] < tid)) {
			waiting_time = waiting_time_vector[region];
		}

		goto retry;

	}
	log_info(NC, "Lock on region %d acquired by event %f\n", region, time);

}

/*
void min_output_time(simtime_t time) {
	outgoing_time_vector[tid] = time;
}
*/

/*void commit_time(void) {
	current_time_vector[tid] = INFTY;
}*/

int check_safety(simtime_t time) {
	unsigned int i;

	//  while(__sync_lock_test_and_set(&queue_lock, 1))
	//    while(queue_lock);

	for (i = 0; i < n_cores; i++) {

		if ((i != tid) && ((current_time_vector[i] < time) || (current_time_vector[i] == time && i < tid))) {

			return 0;
//                      min = (current_time_vector[i] < outgoing_time_vector[i] ? current_time_vector[i] : outgoing_time_vector[i]);
//                      min_tid = i;
//                      *events++;
		}

		/*if ((i != tid) && ((current_time_vector[i] < min) || (outgoing_time_vector[i] < min))) {
		   min = (current_time_vector[i] < outgoing_time_vector[i] ? current_time_vector[i] : outgoing_time_vector[i]);
		   min_tid = i;
		   *events++;
		   } */
	}
/*	log_info(YELLOW, "Checking safety for time %f, min is %f hold by thread %d\n", time, min == INFTY ? -1 : min, min_tid);
	if (current_time_vector[tid] < min) {
		ret = 1;
		goto out;
	}
*/
//      if (current_time_vector[tid] == min && tid < min_tid) {
//              ret = 1;
//      }

// out:
	//  __sync_lock_release(&queue_lock);

	return 1;
}

bool check_waiting(simtime_t time) {
	// Check for thread with less event's timestamp
//      log_info(PURPLE, "Event %f is checking if someone else has priority on region %d (%f)\n", time, current_lp, waiting_time_vector[current_lp] == INFTY ? -1 : waiting_time_vector[current_lp]);
	return (waiting_time_vector[current_lp] < time || (waiting_time_vector[current_lp] == time && tid > waiting_time_who[current_lp]));
}

void flush(msg_t * msg) {
	unsigned int region;

	log_info(NC, "Flushing event at time %f\n", current_lvt);

	while (__sync_lock_test_and_set(&queue_lock, 1))
		while (queue_lock) ;

	region = msg->receiver_id;

//      outgoing_time_vector[tid] = (t_min != -1 ? t_min : time);
//      current_time_vector[tid] = INFTY;
	while (__sync_lock_test_and_set(&waiting_time_lock[region], 1))
		while (waiting_time_lock[region]) ;

	waiting_time_vector[region] = INFTY;
	waiting_time_who[region] = n_cores;

	__sync_lock_release(&waiting_time_lock[region]);

//      log_info(NC, "Vector status: outgoing=%f\n", t_min);

//      log_info(NC, "Lock on region %d released by event %f\n", region, time);

	__sync_lock_release(&queue_lock);
}
