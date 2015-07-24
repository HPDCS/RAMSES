#pragma once
#ifndef __MESSAGESTATE_H
#define __MESSAGESTATE_H

#include "simtypes.h"

bool check_waiting(simtime_t time);

void message_state_init(void);

// It sets the actual execution time in the current thread
void execution_time(msg_t * msg);

// It returns 1 if there is not any other timestamp less than "time" in the timestamps executed and in all outgoing messages.
int check_safety(simtime_t time);

void flush(msg_t * msg);

#endif
