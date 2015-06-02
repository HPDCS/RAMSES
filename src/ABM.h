#pragma once
#ifndef __ABM_H
#define __ABM_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
#include <stdbool.h>


#ifdef INIT
 #undef INIT
#endif
/// This is the message code which is sent by the simulation kernel upon startup
#define INIT	0


/// This macro can be used to convert command line parameters to integers
#define parseInt(s) ({\
			int __value;\
			char *__endptr;\
			__value = (int)strtol(s, &__endptr, 10);\
			if(!(*s != '\0' && *__endptr == '\0')) {\
				fprintf(stderr, "%s:%d: Invalid conversion value: %s\n", __FILE__, __LINE__, s);\
			}\
			__value;\
		     })


/// This macro can be used to convert command line parameters to doubles
#define parseDouble(s) ({\
			double __value;\
			char *__endptr;\
			__value = strtod(s, &__endptr);\
			if(!(*s != '\0' && *__endptr == '\0')) {\
				fprintf(stderr, "%s:%d: Invalid conversion value: %s\n", __FILE__, __LINE__, s);\
			}\
			__value;\
		       })


/// This macro can be used to convert command line parameters to floats
#define parseFloat(s) ({\
			float __value;\
			char *__endptr;\
			__value = strtof(s, &__endptr);\
			if(!(*s != '\0' && *__endptr == '\0')) {\
				fprintf(stderr, "%s:%d: Invalid conversion value: %s\n", __FILE__, __LINE__, s);\
			}\
			__value;\
		       })


/// This macro can be used to convert command line parameters to booleans
#define parseBoolean(s) ({\
			bool __value;\
			if(strcmp((s), "true") == 0) {\
				__value = true;\
			} else {\
				__value = false;\
			}\
			__value;\
		       })


/// This defines the type with whom timestamps are represented
typedef double simtime_t;

/// Infinite timestamp: this is the highest timestamp in a simulation run
#define INFTY DBL_MAX


// Topology library
#define TOPOLOGY_HEXAGON	1000
#define TOPOLOGY_SQUARE		1001
#define TOPOLOGY_MESH		1002
#define TOPOLOGY_STAR		1003
#define TOPOLOGY_RING		1004
#define TOPOLOGY_BIDRING	1005
#define TOPOLOGY_GRAPH		1006

extern void SetupGraph(const char *graph_file);



// Expose to the application level the rollbackable numerical library
double Random(void);
int RandomRange(int min, int max);
int RandomRangeNonUniform(int x, int min, int max);
double Expent(double mean);
double Normal(void);
double Gamma(int ia);
double Poisson(void);
int Zipf(double skew, int limit);


/* FUNCTIONS TYPEDEFS */
typedef void *(*init_f)(unsigned int id);
typedef void (*interaction_f)(unsigned int a, unsigned int b, simtime_t now, void *args, size_t size);
typedef void (*update_f)(unsigned int r, simtime_t now, void *args, size_t size);


/* CORE API */

/**
 * Called once at the beginning to setup the whole simulation engine.
 *
 * @param agentc Number of the agents the engine has to handle
 * @param agent_init Pointer to the function callback the simulation engine will invoke to initialize each agent's state
 * @param regionc Number of the regions the engine has to handle
 * @param reigon_init Pointer to the function callback the simulation engine will invoke to initialize each region's state
 */
extern void Setup(unsigned int agentc, init_f agent_init, unsigned int regionc, init_f region_init);

/**
 * Sets the initial region where the current agent starts from.
 *
 * @param region The region's id
 */
extern void InitialPosition(unsigned int region);

/**
 * Once set up the simulatione engine, it starts the simulation using the specified number of threads
 *
 * @param n_threads Number of threads to use
 */
extern void StartSimulation(unsigned short int n_threads);
extern void Move(unsigned int agent, unsigned int destination, simtime_t time);
extern void AgentInteraction(unsigned int agent_a, unsigned int agent_b, simtime_t time, interaction_f agent_interaction, void *args, size_t size); 
extern void EnvironmentInteraction(unsigned int agent, unsigned int region, simtime_t time, interaction_f environment_interaction, void *args, size_t size);
extern void EnvironmentUpdate(unsigned int region, simtime_t time, update_f environment_update, void *args, size_t size);

/**
 * Returns the current agent's state.
 *
 * @param agent_id The id of the target agent.
 * @return A pointer to the state's structure
 */
extern void *GetAgentState(unsigned int agent_id);

/**
 * Return the currente region's state.
 * 
 * @param region_id The id of the target region.
 * @return A pointer to the state's structure.
 */
extern void *GetRegionState(unsigned int region_id);

/**
 * Facility to find out all the mates that are in the same region of the current agent.
 *
 * @param neighbours Pointer to an array of integers where to put the IDs of all the neighbours.
 * @return Returns the number of the agents close to the caller one, namely the size of the array.
 */
extern int GetNeighbours(unsigned int **neighbours);

/**
 * Returns a random reachable region given a topology specification.
 *
 * @param topology Specifies current map's topology.
 * @return The id of a randomly chosen region towrd which the agent can move.
 */
extern unsigned int FindRegion(int topology);

/**
 * Given a current region and a direction (coherent with the current topology)
 * the simulation engine will provide the target region toward which the agent
 * wants to move on.
 *
 * @param region_id Is the current region's id where the agent actually is.
 * @param direction Integer number defining the agent's heading.
 * @return The target region's id or -1 in case no cell are reachable with this configuration.
 */
extern unsigned int GetTargetRegion(unsigned int region_id, unsigned int direction);


#endif /* __ABM_H */

