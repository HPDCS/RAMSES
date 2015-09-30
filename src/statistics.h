#ifndef __STATISTICS_H__
#define __STATISTICS_H__

#define STAT_SAFE_TIME	0
#define STAT_SAFE_COUNT	1
#define STAT_REV_TIME	2
#define STAT_REV_COUNT	3
#define STAT_UNDO_GEN_TIME	4
#define STAT_UNDO_GEN_COUNT	5
#define STAT_UNDO_EXE_TIME	6
#define STAT_UNDO_EXE_COUNT	7
#define STAT_UNDO_MEAN_SIZE 8

typedef struct _stats {
	double safe_time;
	int safe_count;
	double rev_time;
	int rev_count;

	double gen_time;
	int gen_count;
	double exe_time;
	int exe_count;

	int reverse_window_size;
	int undo_event_mean_size;
} stats;


void init_stats(void);

void stat_post(unsigned int tid, unsigned int type, double value);

void gather_stats(stats *report);

#endif // __STATISTICS_H__