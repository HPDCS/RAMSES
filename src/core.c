#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <pthread.h>
#include <string.h>
#include <immintrin.h>
#include <stdarg.h>
#include <limits.h>

#include <RAMSES.h>
#include <dymelor.h>
#include <numerical.h>
#include <timer.h>

#include "core.h"
#include "message.h"
#include "function_map.h"

#include "reverse.h"


#define HILL_CLIMB_EVALUATE	500
#define DELTA 500		// tick count
#define HIGHEST_COUNT	5


simtime_t *processing;
simtime_t *wait_time;
unsigned int *wait_who;
int *waiting_time_lock;
int *region_lock;

extern int queue_lock;

// Statistics
unsigned int rollbacks = 0;
unsigned int safe = 0;
unsigned int unsafe = 0;

unsigned short int number_of_threads = 1;

unsigned int agent_c = 0;
unsigned int region_c = 0;

unsigned int *agent_position;
bool **presence_matrix;		// Rows are cells, columns are agents

__thread simtime_t current_lvt = 0;
__thread unsigned int current_lp = 0;	/// Represents the region's id
__thread unsigned int tid = 0;	/// Logical id of the worker thread (may be different from the region's id)

/* Total number of cores required for simulation */
unsigned int n_cores;		// TODO: ?

bool stop = false;
bool sim_error = false;

void **states;			/// Linear vector which holds pointers to regions' (first) and agents' states
bool *can_stop;

unsigned int evt_count;

extern void move(unsigned int agent, unsigned int destination);

void rootsim_error(bool fatal, const char *msg, ...) {
	char buf[1024];
	va_list args;

	va_start(args, msg);
	vsnprintf(buf, 1024, msg, args);
	va_end(args);

	fprintf(stderr, (fatal ? "[FATAL ERROR] " : "[WARNING] "));

	fprintf(stderr, "%s", buf);
	fflush(stderr);

	if (fatal) {
		// Notify all KLT to shut down the simulation
		sim_error = true;
		exit(EXIT_FAILURE);
	}
}


static msg_t *fetch(void) {
	unsigned int region;
	simtime_t time;
	simtime_t waiting_time;
	msg_t *msg = NULL;

	// Gets the minimum timestamp event from the queue
	while (__sync_lock_test_and_set(&queue_lock, 1) == 1)
		while (queue_lock);

	msg = get_min_timestamp_event();
	if (msg == NULL) {
		printf("NULL\n");
		processing[tid] = INFTY;
		__sync_lock_release(&queue_lock);
		return NULL;
	}

	processing[tid] = msg->timestamp;

	__sync_lock_release(&queue_lock);

	log_info(NC, "Get event with time %f\n", msg->timestamp);

	time = msg->timestamp;

	// Gets the lock on the region
	region = msg->receiver_id;
//	printf("message: %p region: %d wait_who: %p\n", msg, region, wait_who);

	log_info(NC, "Event with time %f tring to acquired lock on region %d\n", time, region);

 retry:
	if (__sync_lock_test_and_set(&region_lock[region], 1) == 1) {
		// If the thread cannot acquire the lock, this means that another
		// one is performing an event on that region; it has to register
		// its event's timestamp on the waiting queue, if and only if that
		// time is less then the possible already registerd one.

		while (__sync_lock_test_and_set(&waiting_time_lock[region], 1) == 1)
			while (waiting_time_lock[region]) ;

		waiting_time = wait_time[region];

		if (time < waiting_time || (time == waiting_time && tid < wait_who[region])) {

			wait_time[region] = time;
			wait_who[region] = tid;
		}

		__sync_lock_release(&waiting_time_lock[region]);

		// Spins over the wait_who until the current event time is grater
		// than the waiting one

		while ((time > waiting_time) || (time == waiting_time && wait_who[region] < tid)) {
			waiting_time = wait_time[region];
		}

		goto retry;

	}
	log_info(NC, "Lock on region %d acquired by event %f\n", region, time);


	return msg;
}


int check_safety(simtime_t time) {
	unsigned int i;

	for (i = 0; i < n_cores; i++) {

		if ((i != tid) && ((processing[i] < time) || (processing[i] == time && i < tid))) {

			return 0;
		}
	}

	return 1;
}

bool check_waiting(simtime_t time) {
	// Check for thread with less event's timestamp
//      log_info(PURPLE, "Event %f is checking if someone else has priority on region %d (%f)\n", time, current_lp, wait_time[current_lp] == INFTY ? -1 : wait_time[current_lp]);
	return (wait_time[current_lp] < time || (wait_time[current_lp] == time && tid > wait_who[current_lp]));
}

void flush(msg_t * msg) {
	unsigned int region;

	log_info(NC, "Flushing event at time %f\n", current_lvt);

	while (__sync_lock_test_and_set(&queue_lock, 1) == 1)
		while (queue_lock) ;

	region = msg->receiver_id;

	while (__sync_lock_test_and_set(&waiting_time_lock[region], 1) == 1)
		while (region_lock[region]) ;

	if(wait_who[region] == tid) {
		wait_time[region] = INFTY;
		wait_who[region] = n_cores;
	}

	__sync_lock_release(&waiting_time_lock[region]);
	
	// TODO: la flush_messages serve per permettere l'inserimento di nuovi
	// eventi nella calqueue. Da verificare dove spostarla
	flush_messages();

//      log_info(NC, "Vector status: outgoing=%f\n", t_min);

//      log_info(NC, "Lock on region %d released by event %f\n", region, time);

	__sync_lock_release(&queue_lock);
}




// Main loop
void thread_loop(void) {
	int type;
	msg_t *current_m;
	unsigned int current;
	revwin *window = NULL;

#ifdef FINE_GRAIN_DEBUG
	unsigned int non_transactional_ex = 0, transactional_ex = 0;
#endif

	window = create_new_revwin(0);
	printf("window is at %p\n", window);

	while (!stop && !sim_error) {

		current_lp = UINT_MAX;
		current_lvt = INFTY;

		current_m = fetch();

		if (current_m == NULL) {
			continue;
		}

		current_lp = current_m->receiver_id;
		current_lvt = current_m->timestamp;
		type = current_m->type;

 reexecute:

		if (check_safety(current_lvt) == 1) {

			safe++;
			log_info(CYAN, "Event at time %f is safe\n", current_lvt);

			if (type == EXECUTION_Move) {
				current = agent_position[current_m->receiver_id];
				move(current_m->entity1, current_m->receiver_id);
			} else
				call_regular_function(current_m);

			log_info(NC, "Event at time %f has been processed\n", current_lvt);

#ifdef FINE_GRAIN_DEBUG
			__sync_fetch_and_add(&non_transactional_ex, 1);
#endif
		} else {

			unsafe++;

			log_info(RED, "Event at time %f is not safe: running in reversible mode\n", current_m->timestamp);

			// Reset the reverse window
			reset_window(window);

			if (type == EXECUTION_Move) {
				current = agent_position[current_m->receiver_id];
				move(current_m->entity1, current_m->receiver_id);
			} else {
				call_instrumented_function(current_m);
			}

			// Tries to commit actual event until thread finds out that
			// someone else is waiting for the same region (current_lp)
			// with a less timestamp. If this is the case, it does a rollback.
			log_info(NC, "Event %f waits for commit\n", current_m->timestamp);

			while (1) {
				// If the event is not yet safe continue to retry it safety
				// hoping that commit horizion eventually will progress
				if (check_safety(current_lvt) == 1) {
					log_info(GREEN, "Event at time %f has became safe: flushing...\n", current_m->timestamp);
					break;
				} else {
					// If some other thread is wating with a less event's timestp,
					// then run a rollback and exit
					if (check_waiting(current_m->timestamp) == 1) {
						log_info(YELLOW, "Event at time %f must be undone: revesing...\n", current_m->timestamp);

						rollbacks++;
						if (current_m->type == EXECUTION_Move) {
							// If the event is a move, than it can be handled entirely here
							move(current_m->entity1, current);
							reset_outgoing_msg();
							__sync_lock_release(&region_lock[current_m->receiver_id]);
							goto reexecute;
						}

						execute_undo_event(window);

						reset_outgoing_msg();
						__sync_lock_release(&region_lock[current_m->receiver_id]);
						goto reexecute;
					}
				}
			}

			log_info(NC, "Reverse window has been released\n");
		}

		flush(current_m);
		__sync_lock_release(&wait_who[current_m->receiver_id]);
		free(current_m);

		if (tid == 0) {
			evt_count++;
			if ((evt_count - 100 * (evt_count / 100)) == 0)
				printf("TIME: %f\n", current_lvt);
		}
	}

	processing[tid] = INFTY;

#ifdef FINE_GRAIN_DEBUG

	printf("Thread %d executed in non-transactional block: %d\n" "Thread executed in transactional block: %d\n", tid, non_transactional_ex, transactional_ex);
#endif

}
