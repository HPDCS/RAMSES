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


void message_state_init(void)
{
  unsigned int i;
  
  current_time_vector = malloc(sizeof(simtime_t) * n_cores);
  outgoing_time_vector = malloc(sizeof(simtime_t) * n_cores);
  waiting_time_vector = malloc(sizeof(simtime_t) * region_c);
  region_lock = malloc(sizeof(int) * region_c);
  
  for(i = 0; i < n_cores; i++)	// TODO: n_cores -> ?
  {
    current_time_vector[i] = INFTY;
    outgoing_time_vector[i] = INFTY; //TODO: Oppure a 0 ? riguardare...
  }
}

void execution_time(simtime_t time) {
	unsigned int region;
	simtime_t waiting_time;
	bool retry;

	// Gets the lock on the region
	region = current_msg.receiver_id;

	printf("[%u] INFO: Event with time %f tring to acquired lock on region %d\n", tid, time, region);

	if(__sync_lock_test_and_set(&region_lock[region], 1) == 1) {
		// If the thread cannot acquire the lock, this means that another
		// one is performing an event on that region; it has to register
		// its event's timestamp on the waiting queue, if and only if that
		// time is less then the possible already registerd one.

		do {
			waiting_time = waiting_time_vector[region];
			retry = false;
			
			printf("[%u] INFO: Event %f is waiting for region %d\n", tid, time, region);

			if(waiting_time < time) {

				retry = true;

				// Register the current event's timestamp in the waiting queue via CAS
				if(__sync_bool_compare_and_swap(UNION_CAST(&waiting_time, long long *), waiting_time, time))
					break;
			}
			
		} while(retry);

		// Spins over the region_lock until the current event time is grater
		// than the waiting one
		while(time < waiting_time) {
			while(__sync_lock_test_and_set(&region_lock[region], 1) == 1);
		}
	}

	printf("[%u] INFO: Lock on region %d acquired by event %f\n", tid, region, time);
	
	current_time_vector[tid] = time;
	outgoing_time_vector[tid] = INFTY;

	__sync_lock_release(&region_lock[region]);

	printf("[%u] INFO: Lock on region %d released by event %f\n", tid, region, time);
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

int check_safety(simtime_t time, unsigned int *events)
{
  int i;
  unsigned int min_tid = n_cores + 1;
  double min = INFTY;
  int ret = 0;

  *events = 0;

//  while(__sync_lock_test_and_set(&queue_lock, 1))
//    while(queue_lock);
  
  for(i = 0; i < n_cores; i++)
  {

    if( (i != tid) && ((current_time_vector[i] < min) || (outgoing_time_vector[i] < min)) )
    {
      min = ( current_time_vector[i] < outgoing_time_vector[i] ?  current_time_vector[i] : outgoing_time_vector[i]  );
      min_tid = i;
      *events++;
    }
  }

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
	return (waiting_time_vector[current_lp] < time);
}


void flush(void) {
  double t_min;
  while(__sync_lock_test_and_set(&queue_lock, 1))
    while(queue_lock);

  t_min = queue_deliver_msgs();

  current_time_vector[tid] = INFTY;
  outgoing_time_vector[tid] = t_min;

  __sync_lock_release(&queue_lock);
}



