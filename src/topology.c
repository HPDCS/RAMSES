#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <core.h>
#include <ABM.h>

double *pmove;			// Probability matrix of each move (Source region by Destination region)
unsigned int topology;

static unsigned int get_target_id(unsigned int region_id, unsigned int direction) {
	// receiver is not unsigned, because we exploit -1 as a border case in the bidring topology.
	int target;

	// These must be unsigned. They are not checked for negative (wrong) values,
	// but they would overflow, and are caught by a different check.
	unsigned int edge;
	unsigned int x, y, nx, ny;

	switch (topology) {

	case TOPOLOGY_HEXAGON:

#define NW	0
#define W	1
#define SW	2
#define SE	3
#define E	4
#define NE	5

		// Convert linear coords to hexagonal coords
		edge = sqrt(region_c);
		x = region_id % edge;
		y = region_id / edge;

		// Sanity check!
		if (edge * edge != region_c) {
			rootsim_error(true, "Hexagonal map wrongly specified!\n");
			return 0;
		}
		// Very simple case!
		if (region_c == 1) {
			target = region_id;
			break;
		}

		bool invalid = false;

		if (direction > 6)
			rootsim_error(true, "Wrong direction for this topology");

		// Find the neighbour towards 'direction'
		do {
			if (invalid) {
				target = (target + 1) % 6;
			}

			switch (direction) {
			case NW:
				nx = (y % 2 == 0 ? x - 1 : x);
				ny = y - 1;
				break;
			case NE:
				nx = (y % 2 == 0 ? x : x + 1);
				ny = y - 1;
				break;
			case SW:
				nx = (y % 2 == 0 ? x - 1 : x);
				ny = y + 1;
				break;
			case SE:
				nx = (y % 2 == 0 ? x : x + 1);
				ny = y + 1;
				break;
			case E:
				nx = x + 1;
				ny = y;
				break;
			case W:
				nx = x - 1;
				ny = y;
				break;
			default:
				rootsim_error(true, "Met an impossible condition at %s:%d. Aborting...\n", __FILE__, __LINE__);
			}

			invalid = true;

			// We don't check is nx < 0 || ny < 0, as they are unsigned and therefore overflow
		} while (nx >= edge || ny >= edge);

		// Convert back to linear coordinates
		target = (ny * edge + nx);

#undef NE
#undef NW
#undef W
#undef SW
#undef SE
#undef E

		break;

	case TOPOLOGY_SQUARE:

#define N	0
#define W	1
#define S	2
#define E	3

		// Convert linear coords to square coords
		edge = sqrt(region_c);
		x = region_id % edge;
		y = region_id / edge;

		// Sanity check!
		if (edge * edge != region_c) {
			rootsim_error(true, "Square map wrongly specified!\n");
			return 0;
		}
		// Very simple case!
		if (region_c == 1) {
			target = region_id;
			break;
		}
		// Find a random neighbour
		switch (direction) {
		case N:
			nx = x;
			ny = y - 1;
			break;
		case S:
			nx = x;
			ny = y + 1;
			break;
		case E:
			nx = x + 1;
			ny = y;
			break;
		case W:
			nx = x - 1;
			ny = y;
			break;
		default:
			printf("Direction %d unknown\n", direction);
			rootsim_error(true, "Met an impossible condition at %s:%d. Aborting...\n", __FILE__, __LINE__);
		}

		if (nx >= edge || ny >= edge)
			return -1;

		// Convert back to linear coordinates
		target = (ny * edge + nx);

#undef N
#undef W
#undef S
#undef E

		break;

		// Direction does not affect the decision on the following topologies which is always random
	case TOPOLOGY_MESH:

		target = (int)(region_c * direction);
		break;

	case TOPOLOGY_BIDRING:

		if (Random() < 0.5) {
			target = region_id - 1;
		} else {
			target = region_id + 1;
		}

		if (target == -1) {
			target = region_c - 1;
		}
		// Can't be negative from now on
		if ((unsigned int)target == region_c) {
			target = 0;
		}

		break;

	case TOPOLOGY_RING:

		target = region_id + 1;

		if ((unsigned int)target == region_c) {
			target = 0;
		}

		break;

	case TOPOLOGY_STAR:

		if (region_id == 0) {
			target = (int)(region_c * Random());
		} else {
			target = 0;
		}

		break;

	case TOPOLOGY_GRAPH:
		// Throws a probability to randomly choose a new move.
		// The algorithm compares the probability with the stack move's probability

		// Gets the current region
		region_id = agent_position[region_id];

		// Rolls the dice
		double prob = Random();
		double stack = 0;
		unsigned int index;

		for (index = 0; index < region_c; index++) {
			stack += pmove[region_id * region_c + index];
			if (prob < stack) {
				// This direction has been chosen
				target = index;
			}
		}

		break;

	default:
		rootsim_error(true, "Wrong topology code specified: %d. Aborting...\n", topology);
	}

	return (unsigned int)target;
}

/**
 * Parses an input configuration file to setup the graph topology.
 * The file structure must be the following:
 * Source-node \t Destination-node \t Probability [0,1] to move form source to destination
 *
 * Note: this function will be executed once by the master thread (region's id == 0).
 *
 * @param filename The path of the configuration file
 */
void SetupGraph(const char *filename) {
	FILE *file;
	unsigned int source, destination;
	double probability;
	unsigned int row, col;
	double sum;

	// Check if this thread is the main one (region's id == 0)
	if (tid != 0)
		return;

	// Open the file
	file = fopen(filename, "r");
	if (file == NULL) {
		fprintf(stderr, "Error on opening the input confguration graph file\n");
		exit(1);
	}
	// Allocates memory for the probability matrix
	pmove = malloc(sizeof(double) * region_c * region_c);
	bzero(pmove, region_c * region_c);

	// Parses line by line
	int match;
	do {

		match = fscanf(file, "%u\t%u\t%lf\n", &source, &destination, &probability);

		if (match < 0) {
			break;
		} else if (match < 3) {
			fprintf(stderr, "Wrong graph specification in configuration file\n");
			return;
		}

		pmove[source * region_c + destination] = probability;

	} while (match > 0);

	// Close the configuration file
	fclose(file);

	// Sanity check to verify whether the graph has been properly configured,
	// namely the sum of each row must be 1
	for (row = 0; row < region_c; row++) {

		sum = 0;
		for (col = 0; col < region_c; col++) {
			sum += pmove[row * region_c + col];
		}

		if (sum > 1) {
			// Probability cannot be granter than one
			fprintf(stderr, "Error on configuration graph: overal probability cannot be granter than 1 (%u -> %u)", row, col);
			exit(1);
		} else if (sum < 1) {
			// Probability may be less than 1, however it can also be a specification error,
			// therefore it is better to notify the user; however the simulation proceeds
			fprintf(stderr, "Warning, the overall probability of each move is less than 1");
		}
	}
}

void UseTopology(unsigned int _topology) {
	topology = _topology;
}

unsigned int FindRegion(int _topology) {

	unsigned int direction;
	int target;

	do {
		// Basiong on the topology a random direction is chosen
		switch (_topology) {
		case TOPOLOGY_HEXAGON:
			direction = RandomRange(0, 5);
			break;

		case TOPOLOGY_SQUARE:
			direction = RandomRange(0, 3);
			break;
		}

		// Retrieve the target region's id
	} while ((target = get_target_id(current_lp, direction)) < 0);

	return target;
}

unsigned int GetTargetRegion(unsigned int region_id, unsigned int direction) {

	if (region_id > region_c) {
		fprintf(stderr, "Wrong region's id");
		exit(1);
	}
	// Retrieve the target region's id
	return get_target_id(region_id, direction);
}
