#include "region.h"
#include "application.h"


unsigned int get_target_id(unsigned int region_id, unsigned int direction) {
	// TODO: stub to be implemented
	return 0;
}

bool is_reachable(unsigned int region, unsigned int neighbour) {
	// TODO: stub to be implemented
	return true;
}


unsigned int map_hexagon_to_linear(unsigned int x, unsigned int y) {
	unsigned int edge;

	edge = sqrt(number_of_regions);

	// Sanity checks
	if(edge * edge != number_of_regions) {
		rootsim_error(true, "Hexagonal map wrongly specified!\n");
	}
	if(x > edge || y > edge) {
		rootsim_error(true, "Coordinates (%u, %u) are higher than maximum (%u, %u)\n", x, y, edge, edge);
	}

	return y * edge + x;
}


void map_linear_to_hexagon(unsigned int linear, unsigned int *x, unsigned int *y) {
	unsigned int edge;

	edge = sqrt(number_of_regions);

	// Sanity checks
	if(edge * edge != number_of_regions) {
		printf("Hexagonal map wrongly specified!\n");
		abort();
	}
	if(linear > number_of_regions) {
		printf("Required cell %u is higher than the total number of cells %u\n", linear, number_of_regions);
		abort();
	}

	*x = linear % edge;
	*y = linear / edge;
}
