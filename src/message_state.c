#include <stdlib.h>
#include <limits.h>

#include <queue.h>
#include "core.h"
#include "message_state.h"



static simtime_t *current_time_vector;
static simtime_t *outgoing_time_vector;
static simtime_t *waiting_time_vector;

extern int queue_lock;
extern int *region_lock;


void message_state_init(void) {
	unsigned int i;

	current_time_vector = malloc(sizeof(simtime_t) * n_cores);
	outgoing_time_vector = malloc(sizeof(simtime_t) * n_cores);
	waiting_time_vector = malloc(sizeof(simtime_t) * region_c);
	region_lock = malloc(sizeof(int) * region_c);

	for(i = 0; i < n_cores; i++) {
		current_time_vector[i] = INFTY;
		outgoing_time_vector[i] = INFTY; //TODO: Oppure a 0 ? riguardare...
	}

	for(i = 0; i < region_c; i++) {
		waiting_time_vector[i] = INFTY;
	}
}

void execution_time(simtime_t time) {
	unsigned int region;
	simtime_t waiting_time;
	bool retry;

	// Gets the lock on the region
	region = current_msg.receiver_id;

	log_info(NC, "Event with time %f tring to acquired lock on region %d\n", time, region);

	if(__sync_lock_test_and_set(&region_lock[region], 1) == 1) {
		// If the thread cannot acquire the lock, this means that another
		// one is performing an event on that region; it has to register
		// its event's timestamp on the waiting queue, if and only if that
		// time is less then the possible already registerd one.

		do {
			waiting_time = waiting_time_vector[region];
			retry = false;
			
			log_info(NC, "Event %f is waiting for region %d (waiting min time is %f)\n", time, region, waiting_time == INFTY ? -1 : waiting_time);

			if(time < waiting_time) {
				retry = true;

				// Register the current event's timestamp in the waiting queue via CAS
				simtime_t tmp;
				if((tmp=__sync_val_compare_and_swap(UNION_CAST(&waiting_time_vector[region], long long *), waiting_time, time)) != time) {
					log_info(NC, "Event %f has been registered for region %d\n", time, region);
					break;
				}
				log_info(NC, "TMP = %f\n", tmp);
			}
			
		} while(retry);

		
		// Spins over the region_lock until the current event time is grater
		// than the waiting one
		while(time >= waiting_time) {
			while(__sync_lock_test_and_set(&region_lock[region], 1) == 1) while(region_lock[region]);
		}
	}

//	printf("[%u] INFO: Lock on region %d acquired by event %f\n", tid, region, time);
	log_info(NC, "Lock on region %d acquired by event %f\n", region, time);
	
	current_time_vector[tid] = time;
	outgoing_time_vector[tid] = INFTY;

//	__sync_lock_release(&region_lock[region]);

//	printf("[%u] INFO: Lock on region %d released by event %f\n", tid, region, time);
//  if(input_tid != tid && outgoing_time_vector[input_tid] == time)
//    outgoing_time_vector[input_tid] = INFTY;
}

void min_output_time(simtime_t time)
{
  outgoing_time_vector[tid] = time;
}

void commit_time(void)
{
  current_time_vector[tid] = INFTY;
}

int check_safety(simtime_t time, unsigned int *events) {
	int i;
	unsigned int min_tid = n_cores + 1;
	double min = INFTY;
	int ret = 0;

	*events = 0;

	//  while(__sync_lock_test_and_set(&queue_lock, 1))
	//    while(queue_lock);

	for(i = 0; i < n_cores; i++) {

		if( (i != tid) && ((current_time_vector[i] < min) || (outgoing_time_vector[i] < min)) ) {
			min = ( current_time_vector[i] < outgoing_time_vector[i] ?  current_time_vector[i] : outgoing_time_vector[i]  );
			min_tid = i;
			*events++;
		}
	}
	log_info(YELLOW, "Checking safety for time %f, min is %f hold by thread %d\n", time, min == INFTY ? -1 : min, min_tid);

	if(current_time_vector[tid] < min) {
		ret = 1;
		goto out;
	}

	if(current_time_vector[tid] == min && tid < min_tid) {
		ret = 1;
	}


	out:
	//  __sync_lock_release(&queue_lock);

	return ret;
}

bool check_waiting(simtime_t time) {
	// Check for thread with less event's timestamp
//	log_info(PURPLE, "Event %f is checking if someone else has priority on region %d (%f)\n", time, current_lp, waiting_time_vector[current_lp] == INFTY ? -1 : waiting_time_vector[current_lp]);
	return (waiting_time_vector[current_lp] < time);
}


void flush(void) {
	double t_min;
	unsigned int region;
	simtime_t time;

	log_info(NC, "Flushing event at time %f\n", current_lvt);
	
	while(__sync_lock_test_and_set(&queue_lock, 1))
		while(queue_lock);

	t_min = queue_deliver_msgs();
	region = current_msg.receiver_id;
	time = current_msg.timestamp;

	outgoing_time_vector[tid] = time;
	current_time_vector[tid] = INFTY;
	waiting_time_vector[region] = INFTY;

	log_info(NC, "Vector status: outgoing=%f\n", t_min);

	log_info(NC, "Lock on region %d released by event %f\n", region, time);

	__sync_lock_release(&queue_lock);
	__sync_lock_release(&region_lock[region]);
}



