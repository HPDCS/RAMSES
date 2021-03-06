#ifndef __CORE_H
#define __CORE_H

#include <RAMSES.h>

#include <stdbool.h>
#include <math.h>
#include <float.h>

#define MAX_LPs	4096

#define MAX_DATA_SIZE		16
#define THR_POOL_SIZE		100

#define D_DIFFER_ZERO(a) (fabs(a) >= DBL_EPSILON)

#define EXECUTION_AgentInteraction		1
#define EXECUTION_EnvironmentInteraction	2
#define EXECUTION_EnvironmentUpdate		3
#define EXECUTION_Move				4

/// Infinite timestamp: this is the highest timestamp in a simulation run
#define INFTY DBL_MAX

/// This defines the type with whom timestamps are represented
typedef double simtime_t;

#define UNION_CAST(x, destType) (((union {__typeof__(x) a; destType b;})x).b)

#define GREEN "\033[0;32m"
#define RED "\033[0;31m"
#define CYAN "\033[0;36m"
#define YELLOW "\033[0;33m"
#define PURPLE "\033[0;35m"
#define NC "\033[0m"
#define do_log_info(color, ...) do {\
	printf(color);\
	printf("[%u] INFO: ", tid);\
	printf(__VA_ARGS__);\
	printf("\033[0m");\
	fflush(stdout);\
	} while(0);

//#define log_info(color, ...) do_log_info(color, __VA_ARGS__)
#define log_info(color, ...) {}

typedef struct __msg_t {
	unsigned int sender_id;
	unsigned int receiver_id;
	simtime_t timestamp;
	int type;
	int entity1;
	int entity2;
	interaction_f interaction;
	update_f update;
	unsigned int data_size;
	unsigned char data[MAX_DATA_SIZE];
} msg_t;

extern __thread simtime_t current_lvt;
extern __thread unsigned int current_lp;
extern __thread unsigned int tid;

extern unsigned int agent_c;
extern unsigned int region_c;

extern unsigned int *agent_position;
extern bool **presence_matrix;	// Rows are cells, columns are agents;

/* Total number of cores required for simulation */
extern unsigned int n_cores;

//Esegue il loop del singolo thread
void thread_loop(void);

extern void rootsim_error(bool fatal, const char *msg, ...);





extern void init(void);
extern void **states;
extern simtime_t *processing;
extern simtime_t *wait_time;
extern unsigned int *wait_who;
extern int *waiting_time_lock;
extern bool *can_stop;
extern bool stop;
extern unsigned short int number_of_threads;
extern int *region_lock;
extern unsigned int rollbacks;
extern unsigned int safe;
extern unsigned int unsafe;




#endif
