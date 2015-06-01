#pragma once
#ifndef _TCAR_H
#define _TCAR_H


#include <ABM.h>
#include <math.h>

/* DISTRIBUZIONI TIMESTAMP */
#define UNIFORME	0
#define ESPONENZIALE 	1

#define DISTRIBUZIONE ESPONENZIALE


#define is_agent(me) (me >= num_cells)


// Topology
#define CELL_EDGES 4

// Movement directions
#define N 0
#define W 1
#define S 2
#define E 3


#define MASK 0x00000001LL         // Mask used to check, set and unset bits
#define NUM_CHUNKS_PER_BLOCK (sizeof(int) * 8)

#define CHECK_BIT(A,I) ( A[(int)((int)(I) / NUM_CHUNKS_PER_BLOCK)] & (MASK << (int)(I) % NUM_CHUNKS_PER_BLOCK) )
#define SET_BIT(A,I) ( A[(int)((int)(I) / NUM_CHUNKS_PER_BLOCK)] |= (MASK << (int)(I) % NUM_CHUNKS_PER_BLOCK) )
#define RESET_BIT(A,I) ( A[(int)((int)(I) / NUM_CHUNKS_PER_BLOCK)] &= ~(MASK << (int)(I) % NUM_CHUNKS_PER_BLOCK) )

#define BITMAP_SIZE(size) ((int)ceil((size)/NUM_CHUNKS_PER_BLOCK) * sizeof(int))
#define BITMAP_NUMBITS(size) (BITMAP_SIZE(size) * NUM_CHUNKS_PER_BLOCK)
#define ALLOCATE_BITMAP(size) (malloc(BITMAP_SIZE(size)))


extern unsigned int number_of_regions;
extern unsigned int number_of_agents;


typedef struct _event_content_type {
	unsigned int coming_from;
	unsigned int cell;
} event_content_type;


typedef struct _cell_state_type{
	unsigned int neighbours[CELL_EDGES];		/// Neighbour cell for each edge
	bool obstacles[CELL_EDGES];					/// Whether the specific edge has an obstacle
	unsigned int *agents;
	unsigned int present_agents;
} cell_state_type;


typedef struct _map_t {
	bool visited;
	unsigned int neighbours[CELL_EDGES];
} map_t;


typedef struct _agent_state_type {
	unsigned int current_cell;
	unsigned int current_direction;
	unsigned int target_cell;
	unsigned int direction;
	unsigned int met_robots;
	unsigned int visited_cells;
	map_t *visit_map;
	unsigned int *a_star_map;
} agent_state_type;


// FIXME: cambiato nome delle seguenti per non confonderle con gli agenti in prossimità
// le loro definizioni si trovano nel file 'region.h'
//bool isValidNeighbour(unsigned int sender, unsigned int neighbour);
//unsigned int GetNeighbourId(unsigned int sender, unsigned int neighbour);


#endif /* _ANT_ROBOT_H */
