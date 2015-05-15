typedef struct _fmap {
	void *original_address;
	void *instrumented_address;
	char function_name[128];
} fmap;



static fmap *function_map;

static char *prog_name;

void initialize_map(char *argv) {
	char cmd[1024];
	int num_functions;
	int i;
	prog_name = argv[0];
	
	// Get the number of functions to initialize the vector
	snprintf(cmd, 1024, "readelf -a %s | grep FUNC | grep -v UND | grep \"_memtrack\" | wc -l > dump", prog_name);
	system(cmd);
	
	// read file and set num_functions
	
	function_map = malloc(sizeof(fmap) * num_functions);
	
	// Get the number of functions to initialize the vector
	snprintf(cmd, 1024, "readelf -a %s | grep FUNC | grep -v UND | grep \"_memtrack\" > dump", prog_name);
	system(cmd);
	
	// strtok on lines
	
	// Get non-instrumented addresses
	for(i = 0; i < num_functions; i++) {
		snprintf(cmd, 1024, "readelf -a %s | grep FUNC | grep -v UND | grep -v \"_memtrack\" | grep %s > dump", prog_name, function_map[i].function_name);
		system(cmd);
		
		// Strtok the line
	}
}


void call_instrumented_function(msg_t *m) {
	// 
}

void call_regular_function(msg_t *m) {
	// select 
}
