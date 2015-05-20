#include <math.h>
#include <limits.h>
#include <strings.h>

#include "application.h"
#include "agent.h"
#include "region.h"


unsigned int opposite_direction_of(unsigned int direction) {
	unsigned int opposite;

	switch(direction) {
		case NE:
			opposite = SW;
			break;
		case E:
			opposite = W;
			break;
		case SE:
			opposite = NW;
			break;
		case SW:
			opposite = NE;
			break;
		case W:
			opposite = E;
			break;
		case NW:
			opposite = SE;
			break;
		default:
			opposite = UINT_MAX;
			break;
	}

	return opposite;
}

char *direction_name(unsigned int direction) {

	switch(direction) {
		case NE:
			return "NE";
			break;
		case E:
			return "E";
			break;
		case SE:
			return "SE";
			break;
		case SW:
			return "SW";
			break;
		case W:
			return "W";
			break;
		case NW:
			return "NW";
			break;
	}
	return "UNKNOWN";
}

double a_star(agent_state_type *state, unsigned int current_cell, unsigned int *good_direction) {
	unsigned int i;
	double min_distance = INFTY;
	double current_distance;
	double dx, dy;
	unsigned int x1, y1, x2, y2;
	unsigned int tentative_cell;
	double distance_increment;

	*good_direction = UINT_MAX;
	SET_BIT(state->a_star_map, current_cell);
	map_linear_to_hexagon(state->target_cell, &x1, &y1);

	for(i = 0; i < 6; i ++) {

		// Is there a cell to reach in this direction?
		if(!is_reachable(current_cell, i)) {
			continue;
		}

		tentative_cell = get_target_id(current_cell, i);

		// We don't simply visit a cell already visited!
		if(CHECK_BIT(state->a_star_map, tentative_cell)) {
			continue;
		}

		// Can I go in that direction?
		if(state->visit_map[current_cell].neighbours[i] != -1) {

			// Is this the target?
			if(current_cell == state->target_cell) {
//				printf("TROVATO! current cell %d target %d\n", current_cell, state->target_cell);
				*good_direction = i;
				return 0.0;
			}

			// Compute the distance from the target if we make that move
			map_linear_to_hexagon(tentative_cell, &x2, &y2);
			dx = x2-x1;
			dy = y2-y1;
			
			distance_increment = a_star(state, tentative_cell, good_direction);
			// TODO: optimize?
			current_distance = sqrt( dx*dx + dy*dy );

			// Is it a good choice?
			if(current_distance < INFTY && current_distance < min_distance) {
				min_distance = current_distance;
				*good_direction = i;
			}
		}
	}
	
	//	if(min_distance < INFTY) {
//		printf("direction: %d\n", *good_direction); fflush(stdout);
//		printf("%d, ", GetNeighbourId(current_cell, *good_direction));
//	}

	// We're getting far!
	//~ if(min_distance > distance) {
		//~ return INFTY;
	//~ }

	return min_distance;
}


unsigned int compute_direction(agent_state_type *state) {
	unsigned int good_direction = UINT_MAX;

	bzero(state->a_star_map, BITMAP_SIZE(number_of_regions));

	a_star(state, state->current_cell, &good_direction);

	//	printf("\n");
/*	unsigned int x1, y1;
	unsigned int x2, y2;
	double min_distance = INFTY;
	double distance;
	double dx, dy;

	map_linear_to_hexagon(state->target_cell, &x1, &y1);

	int i;
	for( i = 0; i < 6; i++) {
		if(isValidNeighbour(state->current_cell, i)) {
			map_linear_to_hexagon(GetNeighbourId(state->current_cell, i), &x2, &y2);

			dx = x1-x2;
                        dy = y2-y1;
			distance = sqrt( dx*dx + dy*dy );

			if(distance < min_distance) {
				min_distance = distance;
				good_direction = i;
			}
		}
	}
*/
	
	return good_direction;
}


unsigned int closest_frontier(agent_state_type *state, unsigned int exclude) {
	unsigned int i, j;
	unsigned int x, y, curr_x, curr_y;
	bool is_reachable;
	double distance;
	double min_distance = INFTY;
	unsigned int target = -1;
	unsigned int curr_cell = state->current_cell;

	map_linear_to_hexagon(curr_cell, &curr_x, &curr_y); 

	for(i = 0; i < number_of_regions; i++) {

		if(!state->visit_map[i].visited) {

			if(i == exclude) {
				// I know this is a frontier, but I don't want to go there
				continue;
			}
			
			map_linear_to_hexagon(i, &x, &y);
			// TODO: optimize
			distance = sqrt((curr_x - x)*(curr_x - x) + (curr_y - y)*(curr_y - y));

			if(distance < min_distance) {
				min_distance = distance;
				target = i;
			}
		}
	}

	return target;
}
