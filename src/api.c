#include <RAMSES.h>
#include <core.h>
#include <stdbool.h>
#include <timer.h>
#include <limits.h>
#include <message.h>
#include <errno.h>
#include <pthread.h>
#include <numerical.h>
#include <dymelor.h>

static timer simulation_start;
static timer simulation_stop;

static init_f agent_initialization;
static init_f region_initialization;

extern double fwd_time, rev_time;

static void process_init_event(void) {
	unsigned int index;
	unsigned int agent;

	// Sets up REGIONS
	current_lvt = 0;
	for (index = 0; index < region_c; index++) {
		// Calls registered callback function to initialize the regions.
		// Callback function will return the pointer to the initialized region's state
		states[index] = (*region_initialization) (index);

		flush_messages();
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

		flush_messages();
	}
}


void init(void) {
	unsigned int i;

	states = malloc(sizeof(void *) * (region_c + agent_c));
	can_stop = malloc(sizeof(bool) * region_c);

	// DEBUG
	fwd_time = rev_time = 0;

#ifndef NO_DYMELOR
	dymelor_init();
#endif

	queue_init();
	numerical_init();
	
	processing = malloc(sizeof(simtime_t) * n_cores);
	wait_time = malloc(sizeof(simtime_t) * region_c);
	wait_who = malloc(sizeof(int) * region_c);
	waiting_time_lock = malloc(sizeof(int) * region_c);
	region_lock = malloc(sizeof(int) * region_c);

	for (i = 0; i < n_cores; i++) {
		processing[i] = INFTY;
	}

	for (i = 0; i < region_c; i++) {
		waiting_time_lock[i] = 0;
		region_lock[i] = 0;
		wait_time[i] = INFTY;
		wait_who[i] = n_cores;
	}

}


void InitialPosition(unsigned int region) {

//	printf("INFO: agent %d set in region %d\n", current_lp, region);

	// NOTE:
	// At this time, current_lp holds the current agent that is
	// being initialized during the setup phase.
	agent_position[current_lp] = region;
	presence_matrix[region][current_lp] = true;
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


// API implementation
void EnvironmentUpdate(unsigned int region, simtime_t time, update_f environment_update, void *args, size_t size) {
//	printf("Schedule EnvironmentUpdate at time %f\n", time);
	insert_message(region, UINT_MAX, UINT_MAX, NULL, environment_update, time, EXECUTION_EnvironmentUpdate, args, size);
}

void EnvironmentInteraction(unsigned int agent, unsigned int region, simtime_t time, interaction_f environment_interaction, void *args, size_t size) {
//	printf("Schedule EnvironmentInteraction at time %f\n", time);
	insert_message(region, agent, UINT_MAX, environment_interaction, NULL, time, EXECUTION_EnvironmentInteraction, args, size);
}

void AgentInteraction(unsigned int agent_a, unsigned int agent_b, simtime_t time, interaction_f agent_interaction, void *args, size_t size) {
//	printf("Schedule AgentInteraction at time %f\n", time);
	insert_message(current_lp, agent_a, agent_b, agent_interaction, NULL, time, EXECUTION_AgentInteraction, args, size);
}

void Move(unsigned int agent, unsigned int destination, simtime_t time) {
//	printf("Schedule Move at time %f\n", time);
	insert_message(destination, agent, UINT_MAX, NULL, NULL, time, EXECUTION_Move, NULL, 0);
}

void move(unsigned int agent, unsigned int destination) {
	// TODO: !!! in caso di rollback non funziona nulla !!!
	unsigned int source;

	source = agent_position[agent];
	agent_position[agent] = destination;
	presence_matrix[destination][agent] = true;
	presence_matrix[source][agent] = false;

	log_info(NC, "Agent %d moved from %d to %d\n", agent, source, destination);
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

	// Final initialization of processes
	init();
	// TODO: da integrare process_init_event() in init()
	process_init_event();

	printf("INFO: Setting up regions and agents...\n");

	// TODO: da integrare in process_init_event()
	// Check whether agents' position has been properly initialized by InitialPosition invocation
	for (i = 0; i < agent_c; i++) {
		if (agent_position[i] == UINT_MAX) {
			// Agent has no position set up, discard it
			rootsim_error(false, "Agent %d has no initial position set up; it will be disposed\n");
			states[i] = NULL;	// TODO: da stabilire come procedere
		}
	}

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
	printf("%d Safe events\n%d Unsafe event\n%d Rollback\n", safe, unsafe, rollbacks);
	printf("Mean time to process safe events: %.3f usec\n", fwd_time);
	printf("Mean time to process reversible events: %.3f usec\n", rev_time);
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

	// init();
	// // TODO: da integrare process_init_event() in init()
	// process_init_event();

	// printf("INFO: Setting up regions and agents...\n");

	// // TODO: da integrare in process_init_event()
	// // Check whether agents' position has been properly initialized by InitialPosition invocation
	// for (i = 0; i < agent_c; i++) {
	// 	if (agent_position[i] == UINT_MAX) {
	// 		// Agent has no position set up, discard it
	// 		rootsim_error(false, "Agent %d has no initial position set up; it will be disposed\n");
	// 		states[i] = NULL;	// TODO: da stabilire come procedere
	// 	}
	// }

}
