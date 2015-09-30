#include <string.h>

#include "statistics.h"
#include "core.h"
#include "reverse.h"

stats *statistics_v;

void init_stats(void) {

	// Initialize the arra of statistics for all threads
	statistics_v = malloc(n_cores * sizeof(stats));
	bzero(statistics_v, n_cores * sizeof(stats));
}

void stat_post(unsigned int tid, unsigned int type, double value) {
	
	switch(type) {

		case STAT_SAFE_TIME:
			statistics_v[tid].safe_time += value;

		case STAT_SAFE_COUNT:
			statistics_v[tid].safe_count++;
			break;

		case STAT_REV_TIME:
			statistics_v[tid].rev_time += value;

		case STAT_REV_COUNT:
			statistics_v[tid].rev_count++;
			break;

		case STAT_UNDO_GEN_TIME:
			statistics_v[tid].gen_time += value;

		case STAT_UNDO_GEN_COUNT:
			statistics_v[tid].gen_count++;
			break;

		case STAT_UNDO_EXE_TIME:
			statistics_v[tid].exe_time += value;

		case STAT_UNDO_EXE_COUNT:
			statistics_v[tid].exe_count++;
			break;

		case STAT_UNDO_MEAN_SIZE:
			if (statistics_v[tid].undo_event_mean_size) {
				statistics_v[tid].undo_event_mean_size = value;
				break;
			}

			statistics_v[tid].undo_event_mean_size += value;
			statistics_v[tid].undo_event_mean_size /= 2;
			break;
	}

}

void gather_stats(stats *report) {
	int i;

	bzero(report, sizeof(stats));

	for (i=0; i < n_cores; i++) {
		report->safe_time += statistics_v[i].safe_time;
		report->safe_count += statistics_v[i].safe_count;
		report->rev_time += statistics_v[i].rev_time;
		report->rev_count += statistics_v[i].rev_count;

		report->gen_time += statistics_v[i].gen_time;
		report->gen_count += statistics_v[i].gen_count;
		report->exe_time += statistics_v[i].exe_time;
		report->exe_count += statistics_v[i].exe_count;

		report->undo_event_mean_size += statistics_v[i].undo_event_mean_size;
	}

	report->undo_event_mean_size /= n_cores;
	report->reverse_window_size = REVERSE_WIN_SIZE;
}