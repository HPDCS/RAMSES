#include "application.h"
#include "agent.h"
#include "region.h"


/**
 * Computes the diff between first and second visit map; therefore
 * it exchanges the computed differences among the two agents.
 *
 * @param agent_a Pointer to the first agent's state
 * @param agent_b Pointer to the second agent's state
 */
void map_diff_exchange(agent_state_type *agent_a, agent_state_type *agent_b) {
	unsigned int index;
	map_t *map_a;
	map_t *map_b;

	// Iterates all over the possible regions
	for (index = 0; index < number_of_regions; index++) {
		// Checks if there is one region such that is visited
		// by one agent and not by the other
		
		map_a = &(agent_a->visit_map[index]);
		map_b = &(agent_b->visit_map[index]);

		// Exchange map informations and recomputes current frontier
		if (map_a->visited && !map_b->visited) {
			memcpy(map_a->neighbours, map_b->neighbours, sizeof(int) * CELL_EDGES);
			map_b->visited = true;
			agent_b->visited_cells++;
			agent_b->target_cell = closest_frontier(agent_b, -1);
			
		} else if (!map_a->visited && map_b->visited) {
			memcpy(map_b->neighbours, map_a->neighbours, sizeof(int) * CELL_EDGES);
			map_a->visited = true;
			agent_a->visited_cells++;
			agent_a->target_cell = closest_frontier(agent_a, -1);
		}

	}
}


void dump_agent_knowledge(agent_state_type *agent) {
	int index;

	for (index = 0; index < number_of_regions; index++) {
		printf("%d ", agent->visit_map[index].visited);
	}
	printf("\tVisited %d regions\n", agent->visited_cells);
}
