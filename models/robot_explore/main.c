#include <stdlib.h>
#include <stdio.h>
#include <ABM.h>

#include <application.h>

unsigned int number_of_agents = 1;
unsigned int number_of_regions = 1;

/**
 * Initializes a new agent with the specified id.
 * Will be allocated a new state to represent the agent.
 *
 * @param id The identification integer number of the new agent
 * @return A pointer to the newly crated state (therefore, agent)
 */
void *agent_init(unsigned int id) {
	agent_state_type *state = NULL;
	
	state = malloc(sizeof(agent_state_type));
	if (state == NULL) {
		// Malloc has failed, no memory can be allocated
		rootsim_error(true, "Unable to allocate memory for a new agent\n");
	}
	bzero(state, sizeof(agent_state_type));
	
	// Initializes visit map
	state->visit_map = malloc(number_of_regions * sizeof(map_t));
	if (state->visit_map == NULL) {
		rootsim_error(true, "Unable to allocate memory for agent's map\n");
	}
	bzero(state->visit_map, number_of_regions * sizeof(map_t));
	
	// Sets the starting region for the current agent
	InitialPosition(0);
	
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
	
	state = malloc(sizeof(region_state_type));
	if (state == NULL) {
		// Malloc has failed, no memory can be allocated
		rootsim_error(true, "Unable to allocate memory for a new region\n");
	}
	bzero(state, sizeof(region_state_type));
	
	// TODO: initialize the region's state
	
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
 * @param size Size of the arguments vector ?
 */
void agent_interaction(unsigned int agent_a, unsigned int agent_b, simtime_t now, void *args, size_t size) {
	// TODO: retrieve agent's state
	agent_state_type state;
}


/**
 * It represents the callback function invoked by the simulation engine
 * whenever a new environmental interaction event has been processed.
 * 
 * @param region_id The identification region's number
 * @param agent_id The identification number of the involed agent
 * @param now Current simulation time
 * @param args Pointer to a arguments vector
 * @param size Size of the arguments vector ?
 */
void region_interaction(unsigned int region_id, unsigned int agent_id, simtime_t now, void *args, size_t size) {
	// TODO: retrieve agent's state
	// TODO: retrieve region's state
	agent_state_type agent;
	cell_state_type region;
	map_t map;
	
	region =  GetRegionState(region_id);
	agent = GetAgentState(agent_id);
	
	// One robot is interacting with the current region, therefore
	// it has to update its visited cell map, check whether it has
	// reached its target or choose a new direction to follow.
	// Finally if in the current region there is another robot,
	// the current one should interact with it by exchanging their maps.
	
	
	// Looks for intreresting neighbour cells, i.e. the one
	// without obstacles, with robots or near to target
	map = agent->visit_map[region_id];
	if (!map->visited) {
		// If the region has not yet visited, then mark it and increment the counter
		map->visited = true;
		agent->visited_cells++;
		
		// Updates the region's neighbours from the robot's perspective
		memcpy(region->neighbours, map->neighbours, sizeof(unsigned int) * 6);
	}
	
	// Checks whether the cell represents robot's final target,
	// otherwise continue randomly
	if (agent->target_cell == region_id) {
		agent->target_cell = UINT_MAX;
	}
	
	// Are there any other robots here in the region
	
	
	// Choose another direction to move towards
	
}


/**
 * It represents the callback function invoked by the simulation engine
 * whenever an environmental update on itself has been processed.
 *
 * @param region_id The identification number of the involed region
 * @param now Current simulation time
 * @param args Pointer to a arguments vector
 * @param size Size of the argument vector
 */
void update_region(unsigned int region_id, simtime_t now, void *args, size_t size) {

}


int main(int argc, argv char**) {
	// Setup of the simulation model
	Setup(number_of_agents, agent_init, number_of_regions, region_init);
	
	// Starting the simulation process
	StartSimulation();

	return 0;
}