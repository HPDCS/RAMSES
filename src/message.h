#ifndef __QUEUE_H
#define __QUEUE_H

#include <RAMSES.h>

typedef struct __msg_t msg_t;

void queue_init(void);
void insert_message(unsigned int receiver, unsigned int entity1, unsigned int entity2, interaction_f interaction, update_f update, simtime_t timestamp, unsigned int event_type, void *event_content, unsigned int event_size);
msg_t *get_min_timestamp_event(void);
double flush_messages(void);
void reset_outgoing_msg(void);

#endif
