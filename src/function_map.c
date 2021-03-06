#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <core.h>
#include <function_map.h>

#include <timer.h>

#include "statistics.h"

typedef struct _fmap {
	void *original_address;
	void *instrumented_address;
	char function_name[128];
} fmap;

static fmap *function_map;
static int num_functions;


// Statics on event processing
__thread int safe_time = 0, rev_time = 0;
__thread unsigned int rev_count = 0, safe_count = 0;

void initialize_map(int argc, char **argv, char **envp) {
	char *prog_name;
	char buff[1024];
	int i = 0;
	prog_name = argv[0];
	FILE *f;
	unsigned long address;
	char *truncate_ptr;
	int dummy_int;
	char dummy_string[128];
	(void)argc;
	(void)envp;
	printf("----------ABMSim Initialization-----------\n");
	printf("My program name is %s\n", prog_name);

	// Get the number of functions to initialize the vector
	snprintf(buff, 1024, "readelf -Wa %s | grep FUNC | grep -v UND | grep \"_reverse$\" | wc -l > dump", prog_name);
	system(buff);

	// read file and set num_functions
	f = fopen("dump", "r");
	fscanf(f, "%d", &num_functions);
	fclose(f);
	printf("Number of reverse functions: %d\n", num_functions);
	function_map = malloc(sizeof(fmap) * num_functions);
	bzero(function_map, sizeof(fmap) * num_functions);

	// Get the number of functions to initialize the vector
	snprintf(buff, 1024, "readelf -Wa %s | grep FUNC | grep -v UND | grep \"_reverse$\" > dump", prog_name);
	system(buff);

	// Get the addresses of reverse functions
	f = fopen("dump", "r");
	while (fgets(buff, 1024, f) != NULL) {

//              printf("\t%s\n", buff);
		sscanf(buff, "%d: %lx %d %s %s %s %d %s\n", &dummy_int, &address, &dummy_int, dummy_string, dummy_string, dummy_string, &dummy_int, function_map[i].function_name);
		function_map[i].instrumented_address = (void *)address;
		truncate_ptr = strstr(function_map[i].function_name, "_reverse");
		if (truncate_ptr == NULL) {
			fprintf(stderr, "%s:%d: Error in initalization\n", __FILE__, __LINE__);
			exit(EXIT_FAILURE);
		}
		*truncate_ptr = '\0';

//              printf("%s at %x\n", function_map[i].function_name, function_map[i].instrumented_address);
		i++;
	}
	fclose(f);

	// Get non-instrumented addresses
	for (i = 0; i < num_functions; i++) {
		snprintf(buff, 1024, "readelf -Wa %s | grep FUNC | grep -v UND | grep -v \"_reverse\" | grep %s > dump", prog_name, function_map[i].function_name);
		system(buff);
		f = fopen("dump", "r");
		if (fgets(buff, 1024, f) == NULL) {
			fprintf(stderr, "%s:%d: Error in initalization\n", __FILE__, __LINE__);
			exit(EXIT_FAILURE);
		}
		sscanf(buff, "%d: %lx %d %s %s %s %d %s\n", &dummy_int, &address, &dummy_int, dummy_string, dummy_string, dummy_string, &dummy_int, dummy_string);
		function_map[i].original_address = (void *)address;
		fclose(f);
		printf("Function %s: original address: %p instrumented address: %p\n", function_map[i].function_name, function_map[i].original_address, function_map[i].instrumented_address);
	}
	unlink("dump");
	printf("----------------COMPLETE-----------------\n");
}

__attribute__ ((section(".preinit_array"))) __typeof__(initialize_map) * __initialize_map = initialize_map;

static void call_it(void *f, msg_t * m) {
	interaction_f interaction;
	update_f update;
	switch (m->type) {
	case EXECUTION_AgentInteraction:
		interaction = (interaction_f) f;
		(*interaction) (m->entity1, m->entity2, m->timestamp, m->data, m->data_size);
		break;
	case EXECUTION_EnvironmentInteraction:
		interaction = (interaction_f) f;
		(*interaction) (m->receiver_id, m->entity1, m->timestamp, m->data, m->data_size);
		break;
	case EXECUTION_EnvironmentUpdate:
		update = (update_f) f;
		(*update) (m->receiver_id, m->timestamp, m->data, m->data_size);
		break;
	case EXECUTION_Move:

		// Manca la callback!
		//(*interaction)(m->entity1, m->entity2, m->timestamp);
		//move(m->entity1, m->entity2);
		break;
	default:
		fprintf(stderr, "%s:%d: Runtime error: unkownw event type %d\n", __FILE__, __LINE__, m->type);
		exit(EXIT_FAILURE);
	}
}

void call_instrumented_function(msg_t * m) {
	void *function = NULL;
	int i;

	timer event_timer;

	if (m->interaction != NULL) {
		function = m->interaction;
	} else if (m->update != NULL) {
		function = m->update;
	} else {
		fprintf(stderr, "%s:%d: Runtime error\n", __FILE__, __LINE__);
		printf("Event type = %d\n", m->type);
		exit(EXIT_FAILURE);
	}
	//log_info(NC, "Finding function at address '%p'\n", function);

	// Find the instrumented function
	for (i = 0; i < num_functions; i++) {
		if (function_map[i].original_address == function) {
			function = function_map[i].instrumented_address;
			goto do_call;
		}
	}
	printf("Function at address '%p' not found!!\n", function);
	fprintf(stderr, "%s:%d: Runtime error\n", __FILE__, __LINE__);
	abort();

 do_call:
 	timer_start(event_timer);
	call_it(function, m);
	double elapsed = (double)timer_value_micro(event_timer);
	stat_post(tid, STAT_REV_TIME, elapsed);
	stat_post(tid, STAT_UNDO_MEAN_SIZE, 0);

	//rev_time[tid] += elapsed;
	//rev_count[tid]++;
}

void call_regular_function(msg_t * m) {
	void *function = NULL;

	timer event_timer;

	if (m->interaction != NULL) {
		function = m->interaction;
	} else if (m->update != NULL) {
		function = m->update;
	} else {
		fprintf(stderr, "%s:%d: Runtime error\n", __FILE__, __LINE__);
		printf("Unable to find function at address %p\n", function);
		exit(EXIT_FAILURE);
	}

	timer_start(event_timer);
	call_it(function, m);
	double elapsed = (double)timer_value_micro(event_timer);
	stat_post(tid, STAT_SAFE_TIME, elapsed);

	//safe_time[tid] += elapsed;
	//safe_count[tid]++;
}
