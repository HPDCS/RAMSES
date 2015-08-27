#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <pthread.h>
#include <string.h>
#include <immintrin.h>
#include <stdarg.h>
#include <limits.h>

#include <ABM.h>
#include <dymelor.h>
#include <numerical.h>
#include <timer.h>

#include "core.h"
#include "queue.h"
#include "simtypes.h"
#include "function_map.h"

#include "reverse.h"

//id del processo principale
#define _MAIN_PROCESS		0
//Le abort "volontarie" avranno questo codice
#define _ROLLBACK_CODE		127


#define HILL_EPSILON_GREEDY	0.05
#define HILL_CLIMB_EVALUATE	500
#define DELTA 500		// tick count
#define HIGHEST_COUNT	5


simtime_t *current_time_vector;
static simtime_t *waiting_time_vector;
static simtime_t *waiting_time_who;
int *waiting_time_lock;

extern int queue_lock;



// Statistics
static timer simulation_start;
static timer simulation_stop;
static unsigned int rollbacks = 0;
static unsigned int safe = 0;
static unsigned int unsafe = 0;

static unsigned short int number_of_threads = 1;

// API callbacks
static init_f agent_initialization;
static init_f region_initialization;

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

void SetState(void *ptr) {
	states[current_lp] = ptr;
}

void *GetRegionState(unsigned int region) {
	return states[region];
}

void *GetAgentState(unsigned int agent) {
	return states[region_c + agent];
}

int GetNeighbours(unsigned int **neighbours) {
	unsigned int region_id = 0;
	unsigned int agent_id = 0;
	unsigned int count = 0;
	int index;

	// Both regions and agents states are coalesced in a single linear
	// list held by 'states'. It first contains regions, then agents

	// In order to find the agents that are in the same regions we have
	// to scan the whole list looking for the position that each agent has.

	// Gets current agent's position
	region_id = current_lp;

	// Counts the other agent that are present in the same region
	count = 0;
	for (agent_id = 0; agent_id < agent_c; agent_id++) {
		if (presence_matrix[region_id][agent_id]) {
			// Increment the total number of agent present in the same region
			count++;
		}
	}

	// Allocates memory for the result list
	*neighbours = malloc(sizeof(unsigned int) * count);

	// Looks for all agents by scanning the list to fetch the exact id of each neighbour
	index = 0;
	for (agent_id = 0; agent_id < agent_c; agent_id++) {
		if (presence_matrix[region_id][agent_id]) {
			// Current agent is actually in the same region,
			// therefore it will be added to the result list
			(*neighbours)[index++] = agent_id;
		}
	}

	return count;
}

void InitialPosition(unsigned int region) {

//	printf("INFO: agent %d set in region %d\n", current_lp, region);

	// At this time, current_lp holds the current agent that is
	// being initialized dureing the setup phase.
	agent_position[current_lp] = region;
	presence_matrix[region][current_lp] = true;
}

static void process_init_event(void) {
	unsigned int index;
	unsigned int agent;

	// Sets up REGIONS
	current_lvt = 0;
	for (index = 0; index < region_c; index++) {
		// Calls registered callback function to initialize the regions.
		// Callback function will return the pointer to the initialized region's state
		states[index] = (*region_initialization) (index);

		queue_deliver_msgs();
	}

	// Sets up AGENTS
	for (agent = 0; agent < agent_c; agent++, index++) {
		// Temporary stores the agent's id in order to use it in the InitialPosition function.
		// Note that this function should be called in the agent_initialization callback, during
		// the initialization phase; therefore it is safe to use 'tid' in the meanwhile, since
		// no thread is actually up and no other execution flow would be started.
		current_lp = agent;

		// Calls registered callback function to initialize the agents.
		// Callback function will return the pointer to the initialized agent's state
		states[index] = (*agent_initialization) (agent);

		queue_deliver_msgs();
	}
}


void init(void) {
//	printf("Initializing internal structures...\n");

	states = malloc(sizeof(void *) * (region_c + agent_c));
	can_stop = malloc(sizeof(bool) * region_c);

#ifndef NO_DYMELOR
	dymelor_init();
#endif

	queue_init();
	numerical_init();

}

bool check_termination(void) {
	unsigned int i;

	bool ret = true;
	for (i = 0; i < region_c; i++) {
		ret &= can_stop[i];
	}

	return ret;
}

// API implementation
void EnvironmentUpdate(unsigned int region, simtime_t time, update_f environment_update, void *args, size_t size) {
//	printf("Schedule EnvironmentUpdate at time %f\n", time);
	queue_insert(region, UINT_MAX, UINT_MAX, NULL, environment_update, time, EXECUTION_EnvironmentUpdate, args, size);
}

void EnvironmentInteraction(unsigned int agent, unsigned int region, simtime_t time, interaction_f environment_interaction, void *args, size_t size) {
//	printf("Schedule EnvironmentInteraction at time %f\n", time);
	queue_insert(region, agent, UINT_MAX, environment_interaction, NULL, time, EXECUTION_EnvironmentInteraction, args, size);
}

void AgentInteraction(unsigned int agent_a, unsigned int agent_b, simtime_t time, interaction_f agent_interaction, void *args, size_t size) {
//	printf("Schedule AgentInteraction at time %f\n", time);
	queue_insert(current_lp, agent_a, agent_b, agent_interaction, NULL, time, EXECUTION_AgentInteraction, args, size);
}

void Move(unsigned int agent, unsigned int destination, simtime_t time) {
//	printf("Schedule Move at time %f\n", time);
	queue_insert(destination, agent, UINT_MAX, NULL, NULL, time, EXECUTION_Move, NULL, 0);
}

static void move(unsigned int agent, unsigned int destination) {
	// TODO: !!! in caso di rollback non funziona nulla !!!
	unsigned int source;

	source = agent_position[agent];
	agent_position[agent] = destination;
	presence_matrix[destination][agent] = true;
	presence_matrix[source][agent] = false;

	log_info(NC, "Agent %d moved from %d to %d\n", agent, source, destination);
}




void message_state_init(void) {
	unsigned int i;

//	printf("n_cores is %d\n", n_cores);

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

	for (i = 0; i < region_c; i++) {
		waiting_time_lock[i] = 0;
		region_lock[i] = 0;
	}
}


void execution_time(msg_t * msg) {
	unsigned int region;
	simtime_t time;
	simtime_t waiting_time;

	time = msg->timestamp;

	// Gets the lock on the region
	region = msg->receiver_id;
//	printf("message: %p region: %d region_lock: %p\n", msg, region, region_lock);

	log_info(NC, "Event with time %f tring to acquired lock on region %d\n", time, region);

 retry:
	if (__sync_lock_test_and_set(&region_lock[region], 1) == 1) {
		// If the thread cannot acquire the lock, this means that another
		// one is performing an event on that region; it has to register
		// its event's timestamp on the waiting queue, if and only if that
		// time is less then the possible already registerd one.

		while (__sync_lock_test_and_set(&waiting_time_lock[region], 1) == 1)
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

int check_safety(simtime_t time) {
	unsigned int i;

	for (i = 0; i < n_cores; i++) {

		if ((i != tid) && ((current_time_vector[i] < time) || (current_time_vector[i] == time && i < tid))) {

			return 0;
		}
	}

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

	while (__sync_lock_test_and_set(&queue_lock, 1) == 1)
		while (queue_lock) ;

	region = msg->receiver_id;

	while (__sync_lock_test_and_set(&waiting_time_lock[region], 1) == 1)
		while (waiting_time_lock[region]) ;

	if(waiting_time_who[region] == tid) {
		waiting_time_vector[region] = INFTY;
		waiting_time_who[region] = n_cores;
	}

	__sync_lock_release(&waiting_time_lock[region]);
	
	// TODO: la queue_deliver_msgs serve per permettere l'inserimento di nuovi
	// eventi nella calqueue. Da verificare dove spostarla
	queue_deliver_msgs();

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

		current_m = queue_min();

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
		__sync_lock_release(&region_lock[current_m->receiver_id]);
		free(current_m);

		//      can_stop[current_lp] = OnGVT(current_lp, states[current_lp]);
		//      stop = check_termination();

		if (tid == 0) {
			evt_count++;
			if ((evt_count - 100 * (evt_count / 100)) == 0)
				printf("TIME: %f\n", current_lvt);
		}
	}

	current_time_vector[tid] = INFTY;

#ifdef FINE_GRAIN_DEBUG

	printf("Thread %d executed in non-transactional block: %d\n" "Thread executed in transactional block: %d\n", tid, non_transactional_ex, transactional_ex);
#endif

}

void *start_thread(void *args) {
	(void)args;

	tid = (int)__sync_fetch_and_add(&number_of_threads, 1);

	thread_loop();

	printf("Thread %d exited\n", tid);

	pthread_exit(NULL);
}

void StartSimulation(unsigned short int app_n_thr) {

	pthread_t p_tid[app_n_thr - 1];
	int ret, i;

	if (region_c == 0) {
		fprintf(stderr, "ERROR: StartSimulation() has been called before Setup(). Aborting...\n");
		exit(EXIT_FAILURE);
	}

	if(app_n_thr < 1) {
		fprintf(stderr, "ERROR: calling StartSimulation with less than 1 thread. Aborting...\n");
		exit(EXIT_FAILURE);
	}

	printf("INFO: Simulation is starting (%d threads)...\n\n", app_n_thr);

	n_cores = app_n_thr;
	message_state_init();

	// Start timer
	timer_start(simulation_start);

	//Child thread
	printf("Starting slave threads... ");
	for (i = 0; i < app_n_thr - 1; i++) {
		if ((ret = pthread_create(&p_tid[i], NULL, start_thread, NULL)) != 0) {
			fprintf(stderr, "%s\n", strerror(errno));
			abort();
		}
		printf("%d ", i+1);
	}
	printf("done\n");

	//Main thread
	thread_loop();

	for (i = 0; i < app_n_thr - 1; i++) {
		pthread_join(p_tid[i], NULL);
	}

	printf("======================================\n");
	printf("Simulation finished\n");
	printf("Overall time elapsed: %ld msec\n", timer_diff_micro(simulation_start, simulation_stop) / 1000);
	printf("%d Safe events\n%d Unsafe event\n%d Rolled back events\n", safe, unsafe, rollbacks);
	printf("======================================\n");
}

void StopSimulation(void) {
	stop = true;

	printf("INFO: Request to STOP Simulation\n");

	timer_start(simulation_stop);
}

void Setup(unsigned int agentc, init_f agent_init, unsigned int regionc, init_f region_init) {
	unsigned int i, j;

	printf("INFO: Setting up simulation platform with %u agents and %u regions...\n", agentc, regionc);

	if (regionc == 0) {
		fprintf(stderr, "ERROR: Starting a simulation with no regions. Aborting...\n");
		exit(EXIT_FAILURE);
	}

	if (agentc == 0) {
		fprintf(stderr, "ERROR: Starting a simulation with no agents. Aborting...\n");
		exit(EXIT_FAILURE);
	}


	// Initialize the two structure which hold region and agent reciprocal positions
	// Note: agent_position is initialized with '-1', therefore it is possible to
	// check whether the application has invoked also the InitialPosition which set it,
	// otherwise the agent will be simply disposed.
	agent_position = malloc(sizeof(unsigned int) * agentc);
	bzero(agent_position, sizeof(unsigned int) * agentc);
	for (i = 0; i < agentc; i++) {
		agent_position[i] = UINT_MAX;
	}

	presence_matrix = malloc(sizeof(bool *) * regionc);
	bzero(presence_matrix, sizeof(bool *) * regionc);
	for (i = 0; i < regionc; i++) {
		presence_matrix[i] = malloc(sizeof(bool) * agentc);
		for (j = 0; j < agent_c; j++) {
			//bzero(presence_matrix[i], sizeof(bool) * agentc);
			presence_matrix[i][j] = false;
		}
	}

	region_c = regionc;
	agent_c = agentc;
	region_initialization = region_init;
	agent_initialization = agent_init;

	init();
	process_init_event();

	printf("INFO: Setting up regions and agents...\n");

	// Check whether agents' position has been properly initialized by InitialPosition invocation
	for (i = 0; i < agent_c; i++) {
		if (agent_position[i] == UINT_MAX) {
			// Agent has no position set up, discard it
			rootsim_error(false, "Agent %d has no initial position set up; it will be disposed\n");
			states[i] = NULL;	// TODO: da stabilire come procedere
		}
	}

}
