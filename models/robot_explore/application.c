#include <stdlib.h>
#include <stdio.h>
#include <strings.h>

#include <ABM.h>

#include "application.h"
#include "agent.h"
#include "region.h"

// Global variables
unsigned int number_of_agents = 4;
unsigned int number_of_regions = 1;

// Declaration of the client side callback functions
void *agent_init(unsigned int id);
void *region_init(unsigned int id);
void agent_interaction(unsigned int agent_a, unsigned int agent_b, simtime_t now, void *args, size_t size);
void region_interaction(unsigned int region_id, unsigned int agent_id, simtime_t now, void *args, size_t size);
void update_region(unsigned int region_id, simtime_t now, void *args, size_t size);

/**
 * Initializes a new agent with the specified id.
 * Will be allocated a new state to represent the agent.
 *
 * @param id The identification integer number of the new agent
 * @return A pointer to the newly crated state (therefore, agent)
 */
void *agent_init(unsigned int id) {
	agent_state_type *state = NULL;
	simtime_t start_time;
	int index;

	printf("APP :: setup agent %d\n", id);

	state = malloc(sizeof(agent_state_type));
	if (state == NULL) {
		// Malloc has failed, no memory can be allocated
		printf("Unable to allocate memory for a new agent\n");
		exit(1);
	}
	bzero(state, sizeof(agent_state_type));
	
	// Initializes visit map
	state->visit_map = malloc(number_of_regions * sizeof(map_t));
	if (state->visit_map == NULL) {
		printf("Unable to allocate memory for a new agent\n");
		exit(1);
	}
	for(index = 0; index < number_of_regions; index++) {
		map_t *map = (state->visit_map + index);
		map->visited = false;
		bzero(map->neighbours, sizeof(unsigned int) * CELL_EDGES);
	}
	
	// Initializes visit map
	state->a_star_map = malloc(number_of_regions * sizeof(map_t));
	if (state->a_star_map == NULL) {
		printf("Unable to allocate memory for a new agent\n");
		exit(1);
	}
	bzero(state->a_star_map, number_of_regions * sizeof(map_t));
	
	// Sets the starting region for the current agent
	// It randomly chooses a region to settle the robot
	state->current_cell = RandomRange(0, number_of_regions - 1);
	InitialPosition(state->current_cell);
	
	// Fires the initial interaction event
	start_time = Expent(AGENT_TIME_STEP);
	EnvironmentInteraction(id, state->current_cell, start_time, region_interaction, NULL, 0);
	
	return state;
}


/**
 * Initializes a new region with the specified id.
 * Will be allocated a new state to represent the region.
 *
 * @param id The identification integer number of the new reigon
 * @return A pointer to the newly crated state
 */
void *region_init(unsigned int id) {
	cell_state_type *state = NULL;
	simtime_t start_time;

	printf("APP :: setup region %d\n", id);
	
	state = malloc(sizeof(cell_state_type));
	if (state == NULL) {
		// Malloc has failed, no memory can be allocated
		printf("Unable to allocate memory for a new agent\n");
		exit(1);
	}
	bzero(state, sizeof(cell_state_type));
	
	// Setup region properties, i.e. obstacles
	// Chooeses randomly if the current region has an obstacle or not
	if (Random() < OBSTACLE_PROB) {
		state->obstacles[RandomRange(0, CELL_EDGES)];
	}
	
	// Fires the initial interaction event
	start_time = 10 * Random();
	EnvironmentUpdate(id, start_time, update_region, NULL, 0);
	
	return state;
}

/**
 * It represents the callback function invoked by the simulation engine
 * whenever a new interaction event between two agents has been processed.
 *
 * @param agent_a First agent that interacts with agent_b
 * @param agent_b Second agent that interacts with agent_a
 * @param now Current simulation time
 * @param args Pointer to a arguments vector
 * @param size Size (in bytes) of the arguments vector
 */
void agent_interaction(unsigned int agent_a, unsigned int agent_b, simtime_t now, void *args, size_t size) {
	// TODO: Da completare con l'inserimento della logica di scambio della mappa?
	// Al momento è gestita da 'region_interaction'
	printf("APP :: region_interaction between agent %d and agent %d\n", agent_a, agent_b);
}


/**
 * It represents the callback function invoked by the simulation engine
 * whenever a new environmental interaction event has been processed.
 * 
 * One robot is interacting with the current region, therefore
 * it has to update its visited cell map, check whether it has
 * reached its target or choose a new direction to follow.
 * Finally if in the current region there is another robot,
 * the current one should interact with it by exchanging their maps.
 *
 * @param region_id The identification region's number
 * @param agent_id The identification number of the involed agent
 * @param now Current simulation time
 * @param args Pointer to a arguments vector
 * @param size Size (in bytes) of the arguments vector
 */
void region_interaction(unsigned int region_id, unsigned int agent_id, simtime_t now, void *args, size_t size) {
	agent_state_type *agent, *mate;
	cell_state_type *region;
	map_t *map;
	
	unsigned int number_of_mates;
	unsigned int *mates;
	unsigned int index;
	unsigned int step_cell;
	simtime_t step_time;
	
	// TODO: come devono essere gestiti i parametri della funzione?

	printf("APP :: region_interaction of agent %d in region %d\n", agent_id, region_id);
	
	// Retrieve the states given the identification numbers of both agent and region
	region = GetRegionState(region_id);
	agent = GetAgentState(agent_id);

	if(agent == NULL) {
		printf("Agent %d has been disposed\n", agent_id);
		return;
	}

	// Update the location of the agent
	agent->current_cell = region_id;
	
	// Updates robot's knowledge
	map = &(agent->visit_map[region_id]);
	if (map == NULL) {
		printf("Visit map is null!\n");
		return;
	}
//	printf("cell %d is visited? %s\n", region_id, map->visited ? "TRUE" : "FALSE");
	if (!map->visited) {
		// If the region has not yet visited, then mark it and increment the counter
		map->visited = true;
		agent->visited_cells++;
		
		// Updates the region's neighbours from the robot's perspective
		memcpy(region->neighbours, map->neighbours, sizeof(unsigned int) * CELL_EDGES);
	}

//	printf("Visited %d cells over %d\n", agent->visited_cells, number_of_regions);
	
	// Checks whether the cell represents robot's final target,
	// otherwise continue randomly
	if (agent->target_cell == region_id) {
		agent->target_cell = UINT_MAX;
	}
	
	// Checks whether there is any other robot in the same region
	if (region->present_agents > 1) {
		// There are other robot here other than me
		
		// Retrieve the list of mates' ids
		number_of_mates = GetNeighbours(&mates);
		
		// I'm going to exchange map's information with each mate
		// i've found in the current region
		for (index = 0; index < number_of_mates; index ++) {
			// TODO: schedulare un evento AgentInteraction invece che gestirlo direttamente quì?
		
			mate = GetAgentState(mates[index]);
			
			// Computes and exchanges map's diff, at the same time
			// when a view exchange takes palce, 'updated' robot's target
			// will be automatically update as well
			map_diff_exchange(agent, mate);
			
			agent->met_robots++;
			mate->met_robots++;
		}
	}
	
	// With some (tiny) probability forget where we are heading to!
	if (Random() < 0.01) { // TODO: to macro
		agent->target_cell = UINT_MAX;
	}
	
	// If there are no target selected, choose a new one
	if (agent->target_cell == UINT_MAX) {
		agent->target_cell = closest_frontier(agent, -1);
		
		// If no good choice is available, choose a random one
		if (agent->target_cell == UINT_MAX) {
			agent->target_cell = RandomRange(0, number_of_regions - 1);
		}
	}
	
	// Compute a direction to move towards
	/*agent->direction = compute_direction(agent);
	
	// If computed direction is UINT_MAX, then there is no path to the target.
	// Just take a random direction
	if(agent->direction == UINT_MAX) {
		do {
			agent->direction = RandomRange(0, 3);
		} while(GetTargetRegion(agent->current_cell, agent->direction) < 0);
	}
	*/
	// Get the region's id from the knowledge of current cell and the chosen direction
	step_cell = FindRegion(TOPOLOGY_SQUARE);//GetTargetRegion(agent->current_cell, agent->direction);
	step_time = now + Expent(AGENT_TIME_STEP);

	// Check termination
	if((double)agent->visited_cells / number_of_regions > .95) {
		StopSimulation();
		
		printf("Robot %d: %.02f percent --- %d meetings so far --- currently in cell %d\n", agent_id,
			(double)agent->visited_cells / number_of_regions * 100, agent->met_robots, agent->current_cell);
		return;
	}

	// Schedule a new intercation event between the agent and the next region
	EnvironmentInteraction(agent_id, step_cell, step_time, region_interaction, NULL, 0);
}


/**
 * It represents the callback function invoked by the simulation engine
 * whenever an environmental update on itself has been processed.
 *
 * @param region_id The identification number of the involed region
 * @param now Current simulation time
 * @param args Pointer to a arguments vector
 * @param size Size (in bytes) of the arguments vector
 */
void update_region(unsigned int region_id, simtime_t now, void *args, size_t size) {
	simtime_t step_time;

	printf("APP :: update_region region %d\n", region_id);
	// Compute the new simulation time at which the vent must be scheduled
	step_time = now + (simtime_t) Expent(REGION_KEEP_ALIVE_INTERVAL);

	// Schedule the new event
	EnvironmentUpdate(region_id, step_time, update_region, NULL, 0);
}


/**
 * It represents the callback invoked by the simulation platform whenever a new
 * movement event has to be processed.
 *
 * @param agent_id The agent's id which is going to move
 * @param region_id The destination region's id the agent would reach
 * @param time Current simulation time
 * @param args Pointer to a arguments vector
 * @param size Size (in bytes) of the arguments vector
 */
void move(unsigned int agent_id, unsigned int region_id, simtime_t time, void* args, size_t size) {
	cell_state_type *region;
	agent_state_type *agent;

	printf("APP :: move action\n");

	// Gets the states
	region = GetRegionState(region);
	agent = GetAgentState(agent);

}


int main(int argc, char** argv) {
	// TODO: gestione dei parametri?

	// TODO: gestire dinamicamente il numero di threads
	unsigned int number_of_threads = 1;

	if (argc > 1) {
		number_of_threads = atoi(argv[1]);
		number_of_agents = atoi(argv[2]);
		number_of_regions = atoi(argv[3]);
	}

	printf("APP :: main\n");

	// Setup the topology
	UseTopology(TOPOLOGY_SQUARE);

	print_topology_map();

	// Setup of the simulation model
	Setup(number_of_agents, agent_init, number_of_regions, region_init);
	
	// Starting the simulation process
	StartSimulation(number_of_threads);

	return 0;
}
