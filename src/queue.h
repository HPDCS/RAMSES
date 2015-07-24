#ifndef __QUEUE_H
#define __QUEUE_H

#include <ABM.h>

/* Struttura dati "Coda con priorità" per la schedulazione degli eventi (provissoria):
 * Estrazione a costo O(n)
 * Dimensione massima impostata a tempo di compilazione
 * Thread-safe (non lock-free)
 */

typedef struct __msg_t msg_t;

void queue_init(void);

void queue_insert(unsigned int receiver, unsigned int entity1, unsigned int entity2, interaction_f interaction, update_f update, simtime_t timestamp, unsigned int event_type, void *event_content, unsigned int event_size);

double queue_pre_min(void);

void queue_register_thread(void);

msg_t *queue_min(void);

int queue_pending_message_size(void);

double queue_deliver_msgs(void);

void queue_destroy(void);

void reset_outgoing_msg(void);

extern int *region_lock;

extern __thread msg_t current_msg __attribute__ ((aligned(64)));

#endif
