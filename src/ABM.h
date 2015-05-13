/**
*			Copyright (C) 2008-2015 HPDCS Group
*			http://www.dis.uniroma1.it/~hpdcs
*
*
* This file is part of ROOT-Sim (ROme OpTimistic Simulator).
*
* ROOT-Sim is free software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; either version 3 of the License, or (at your option) any later
* version.
*
* ROOT-Sim is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with
* ROOT-Sim; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*
* @file ROOT-Sim.h
* @brief This header defines all the symbols which are needed to develop a Model
*        to be simulated on top of ROOT-Sim.
* @author Francesco Quaglia
* @author Alessandro Pellegrini
* @author Roberto Vitali
* @date 3/16/2011
*/



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
#define TOPOLOGY_GRAPH		1005
unsigned int FindRegion(int topology);
void SetupGraph(const char *graph_file);



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
typedef void (*interaction_f)(unsigned int a, unsigned int b, simtime_t now, void *args, size_t size, void *state);
typedef void (*update_f)(unsigned int r, simtime_t now, void *args, size_t size, void *state);


/* CORE API */
extern void Setup(unsigned int agentc, init_f agent_init, unsigned int region, init_f region_init);
extern void InitialPosition(unsigned int region);
extern void StartSimulation(void);
extern void Move(unsigned int destination, simtime_t time);
extern void AgentInteraction(unsigned int agent_a, unsigned int agent_b, simtime_t time, interaction_f agent_interaction, void *args, size_t size); 
extern void EnvironmentInteraction(unsigned int agent, unsigned int region, simtime_t time, interaction_f environment_interaction, void *args, size_t size);
extern void EnvironmentUpdate(unsigned int region, simtime_t time, update_f environment_update, void *args, size_t size);





#endif /* __ABM_H */

