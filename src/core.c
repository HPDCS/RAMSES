#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <pthread.h>
#include <string.h>
#include <immintrin.h>
#include <stdarg.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>

#include <ABM.h>
#include <dymelor.h>
#include <numerical.h>
#include <timer.h>

#include "core.h"
#include "queue.h"
#include "message_state.h"
#include "simtypes.h"

#include "reverse.h"




//id del processo principale
#define _MAIN_PROCESS		0
//Le abort "volontarie" avranno questo codice
#define _ROLLBACK_CODE		127


#define MAX_PATHLEN	512


#define HILL_EPSILON_GREEDY	0.05
#define HILL_CLIMB_EVALUATE	500
#define DELTA 500  // tick count
#define HIGHEST_COUNT	5
__thread int delta_count = 0;
__thread double abort_percent = 1.0;



__thread int execution_state = EXECUTION_IDLE;


unsigned short int number_of_threads = 1;

// API callbacks
init_f agent_initialization;
init_f region_initialization;

interaction_f environment_interaction;
interaction_f agent_interaction;

update_f environment_update;


unsigned int agent_c = 0;
unsigned int region_c = 0;

unsigned int *agent_position;
bool **presence_matrix; // Rows are cells, columns are agents

__thread simtime_t current_lvt = 0;
__thread unsigned int current_lp = 0;
__thread unsigned int tid = 0;


__thread unsigned long long evt_count = 0;
__thread unsigned long long evt_try_count = 0;
__thread unsigned long long abort_count_conflict = 0, abort_count_safety = 0;

unsigned int *lock_vector;			// Tells whether one region has been locked by some thread
unsigned int *waiting_vector;		// Maintains the successor thread waiting for that region
unsigned int *owner_vector;			// Keeps track of the current owner of the region

/* Total number of cores required for simulation */
unsigned int n_cores;
/* Total number of logical processes running in the simulation */
unsigned int n_prc_tot;

bool stop = false;
bool sim_error = false;


void **states;

bool *can_stop;


void rootsim_error(bool fatal, const char *msg, ...) {
	char buf[1024];
	va_list args;

	va_start(args, msg);
	vsnprintf(buf, 1024, msg, args);
	va_end(args);

	fprintf(stderr, (fatal ? "[FATAL ERROR] " : "[WARNING] "));

	fprintf(stderr, "%s", buf);\
	fflush(stderr);

	if(fatal) {
		// Notify all KLT to shut down the simulation
		sim_error = true;
	}
}


/**
* This is an helper-function to allow the statistics subsystem create a new directory
*
* @author Alessandro Pellegrini
*
* @param path The path of the new directory to create
*/
void _mkdir(const char *path) {

	char opath[MAX_PATHLEN];
	char *p;
	size_t len;

	strncpy(opath, path, sizeof(opath));
	len = strlen(opath);
	if(opath[len - 1] == '/')
		opath[len - 1] = '\0';

	// opath plus 1 is a hack to allow also absolute path
	for(p = opath + 1; *p; p++) {
		if(*p == '/') {
			*p = '\0';
			if(access(opath, F_OK))
				if (mkdir(opath, S_IRWXU))
					if (errno != EEXIST) {
						rootsim_error(true, "Could not create output directory", opath);
					}
			*p = '/';
		}
	}

	// Path does not terminate with a slash
	if(access(opath, F_OK)) {
		if (mkdir(opath, S_IRWXU)) {
			if (errno != EEXIST) {
				if (errno != EEXIST) {
					rootsim_error(true, "Could not create output directory", opath);
				}
			}
		}
	}
}


void throttling(unsigned int events) {
  long long tick_count;
  register int i;

  if(delta_count == 0)
	return;

  tick_count = CLOCK_READ();
  while(true) {
	if(CLOCK_READ() > tick_count + 	events * DELTA * delta_count)
		break;
  }
}

void hill_climbing(void) {
	if((double)abort_count_safety / (double)evt_count < abort_percent && delta_count < HIGHEST_COUNT) {
		delta_count++;
//		printf("Incrementing delta_count to %d\n", delta_count);
	} else {
/*		if(random() / RAND_MAX < HILL_EPSILON_GREEDY) {
			delta_count /= (random() / RAND_MAX) * 10 + 1;
		}
*/	}

	abort_percent = (double)abort_count_safety / (double)evt_count;
}


void SetState(void *ptr) {
	states[current_lp] = ptr;
}

void *GetRegionState(unsigned int region) {
	return states[region];
}

void *GetAgentState(unsigned int agent) {
	return states[region_c + agent];
}

// TODO: stub
int GetNeighbours(unsigned int **neighbours) {
	*neighbours = NULL;
	return 0;
}

// TODO: stub
void InitialPosition(unsigned int region) {
	return;
}


static void process_init_event(void) {
  unsigned int i;

  for(i = 0; i < n_prc_tot; i++) {
    current_lp = i;
    current_lvt = 0;
    ProcessEvent(current_lp, current_lvt, INIT, NULL, 0, states[current_lp]);
    queue_deliver_msgs(); 
  }
  
}

void init(unsigned int _thread_num, unsigned int lps_num) {
  printf("Starting an execution with %u threads, %u LPs\n", _thread_num, lps_num);
  n_cores = _thread_num;
  n_prc_tot = lps_num;
  states = malloc(sizeof(void *) * n_prc_tot);
  can_stop = malloc(sizeof(bool) * n_prc_tot);
 
#ifndef NO_DYMELOR
  dymelor_init();
#endif
  queue_init();
  message_state_init();
  numerical_init();
  
//  queue_register_thread();
  process_init_event();
}


bool check_termination(void) {
	int i;
	bool ret = true;
	for(i = 0; i < n_prc_tot; i++) {
		ret &= can_stop[i];
	}
	return ret;
}


// API implementation
void EnvironmentUpdate(unsigned int region, simtime_t time, update_f environment_update, void *args, size_t size) {
	// DC: environment_interaction -> environment_update
	queue_insert(region, UINT_MAX, UINT_MAX, NULL, environment_update, time, EXECUTION_EnvironmentUpdate, args, size);
}

void EnvironmentInteraction(unsigned int agent, unsigned int region, simtime_t time, interaction_f environment_interaction, void *args, size_t size) {
	queue_insert(region, agent, UINT_MAX, environment_interaction, NULL, time, EXECUTION_EnvironmentInteraction, args, size);
}

void AgentInteraction(unsigned int agent_a, unsigned int agent_b, simtime_t time, interaction_f agent_interaction, void *args, size_t size) {
	queue_insert(current_lp, agent_a, agent_b, agent_interaction, NULL, time, EXECUTION_AgentInteraction, args, size);
}

void Move(unsigned int agent, unsigned int destination, simtime_t time) {
	queue_insert(destination, agent, UINT_MAX, NULL, NULL, time, EXECUTION_Move, NULL, 0);
}


void thread_loop(unsigned int thread_id) {
 // int status;
  unsigned int events;
  revwin *window;
  
#ifdef FINE_GRAIN_DEBUG
   unsigned int non_transactional_ex = 0, transactional_ex = 0;
#endif
  
  tid = thread_id;
  
  while(!stop && !sim_error)
  {

    if(queue_min() == 0)
    {
      continue;
    }
    

    current_lp = current_msg.receiver_id;
    current_lvt  = current_msg.timestamp;

      if(check_safety(current_lvt, &events))
      {
	  
	  ProcessEvent(current_lp, current_lvt, current_msg.type, current_msg.data, current_msg.data_size, states[current_lp]);
	
#ifdef FINE_GRAIN_DEBUG
	__sync_fetch_and_add(&non_transactional_ex, 1);
#endif
      }
      else
      {
	  
	  // Create a new revwin to record reverse instructions
	  window = create_new_revwin(0);
	  ProcessEvent_reverse(current_lp, current_lvt, current_msg.type, current_msg.data, current_msg.data_size, states[current_lp]);

	  #ifdef THROTTLING
          throttling(events);
	  #endif
	  
	  while(!check_safety(current_lvt, &events)) {
		  //TODO: check for wait_queue
		  //TODO: check if there are other threads with less timestamp
		 /* if(check_waiting()) {
			execute_undo_event(window);
			break;
		  }*/
	  }
	}
	
	// Free current revwin
	free_revwin(window);
    flush();
 
    can_stop[current_lp] = OnGVT(current_lp, states[current_lp]);
    stop = check_termination();

    #ifdef THROTTLING
    if((evt_count - HILL_CLIMB_EVALUATE * (evt_count / HILL_CLIMB_EVALUATE)) == 0)
	    hill_climbing();
    #endif

    if(tid == _MAIN_PROCESS) {
    	evt_count++;
	if((evt_count - 10000 * (evt_count / 10000)) == 0)
		printf("TIME: %f\n", current_lvt);
    }
        
    //printf("Timestamp %f executed\n", evt.timestamp);
  }
  
  printf("Thread %d aborted %llu times for cross check condition and %llu for memory conflicts\n", tid, abort_count_conflict, abort_count_safety);
  
#ifdef FINE_GRAIN_DEBUG
  
    printf("Thread %d executed in non-transactional block: %d\n"
    "Thread executed in transactional block: %d\n", 
    tid, non_transactional_ex, transactional_ex);
#endif
  
}


void *start_thread(void *args) {
	//~int tid = (int) __sync_fetch_and_add(&number_of_threads, 1);
	tid = (int) __sync_fetch_and_add(&number_of_threads, 1);

	thread_loop(tid);

	pthread_exit(NULL);
}

void StartSimulation(unsigned short int number_of_threads) {
	pthread_t tid[number_of_threads - 1];
	int ret, i;

	if(region_c == 0) {
		fprintf(stderr, "ERROR: StartSimulation() has been called before Setup(). Aborting...\n");
		exit(EXIT_FAILURE);
	}

	//Child thread
	for(i = 0; i < number_of_threads - 1; i++) {
		if((ret = pthread_create(&tid[i], NULL, start_thread, NULL)) != 0) {
			fprintf(stderr, "%s\n", strerror(errno));
			abort();
		}
	}

	//Main thread
	thread_loop(0);

	for(i = 0; i < number_of_threads - 1; i++)
		pthread_join(tid[i], NULL);
}



void Setup(unsigned int agentc, init_f agent_init, unsigned int regionc, init_f region_init) {
	int i;

	if(regionc == 0) {
		fprintf(stderr, "ERROR: Starting a simulation with no regions. Aborting...\n");
		exit(EXIT_FAILURE);
	}

	if(agentc == 0) {
		fprintf(stderr, "ERROR: Starting a simulation with no agents. Aborting...\n");
		exit(EXIT_FAILURE);
	}

	agent_c = agentc;
	agent_initialization = agent_init;
	region_c = regionc;
	region_initialization = region_init;

	agent_position = malloc(sizeof(unsigned int) * agentc);
	bzero(agent_position, sizeof(unsigned int) * agentc);
	presence_matrix = malloc(sizeof(bool *) * regionc);
	for(i = 0; i < regionc; i++) {
		presence_matrix[i] = malloc(sizeof(bool) * agentc);
		bzero(presence_matrix[i], sizeof(bool) * agentc);
	}
}
