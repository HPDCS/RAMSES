#pragma once
#ifndef _TIMER_H
#define _TIMER_H

#include <time.h>
#include <sys/time.h>

/* gettimeofday() timers  - TODO: SHOULD BE DEPRECATED */
typedef struct timeval timer;


#define timer_start(timer_name) gettimeofday(&timer_name, NULL)

#define timer_restart(timer_name) gettimeofday(&timer_name, NULL)

#define timer_value_seconds(timer_name) ((double)timer_value_milli(timer_name) / 1000.0)

#define timer_value_milli(timer_name) ({\
					struct timeval __rs_tmp_timer;\
					int __rs_timedif;\
					gettimeofday(&__rs_tmp_timer, NULL);\
					__rs_timedif = __rs_tmp_timer.tv_sec * 1000 + __rs_tmp_timer.tv_usec / 1000;\
					__rs_timedif -= timer_name.tv_sec * 1000 + timer_name.tv_usec / 1000;\
					__rs_timedif;\
				})

#define timer_value_micro(timer_name) ({\
					struct timeval __rs_tmp_timer;\
					int __rs_timedif;\
				        gettimeofday(&__rs_tmp_timer, NULL);\
					__rs_timedif = __rs_tmp_timer.tv_sec * 1000000 + __rs_tmp_timer.tv_usec;\
					__rs_timedif -= timer_name.tv_sec * 1000000 + timer_name.tv_usec;\
					__rs_timedif;\
				})

#define timer_diff_micro(start, stop) (((stop).tv_sec * 1000000 + (stop).tv_usec) - ((start).tv_sec * 1000000 + (start).tv_usec))

/// string must be a char array of at least 64 bytes to keep the whole string
#define timer_tostring(timer_name, string) do {\
					time_t __nowtime;\
					struct tm *__nowtm;\
					__nowtime = timer_name.tv_sec;\
					__nowtm = localtime(&__nowtime);\
					strftime(string, sizeof string, "%Y-%m-%d %H:%M:%S", __nowtm);\
				} while(0)





/// This overflows if the machine is not restarted in about 50-100 years (on 64 bits archs)
#define CLOCK_READ() ({ \
			unsigned int lo; \
			unsigned int hi; \
			__asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi)); \
			((unsigned long long)hi) << 32 | lo; \
			})

#endif /* _TIMER_H */

