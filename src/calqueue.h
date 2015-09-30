#pragma once
#ifndef __CALQUEUE_H
#define __CALQUEUE_H

//#define CALQSPACE 49153               // Calendar array size needed for maximum resize
#define CALQSPACE 65536		// Calendar array size needed for maximum resize
#define MAXNBUCKETS 32768	// Maximum number of buckets in calendar queue

typedef struct __calqueue_node {
	double timestamp;		// Timestamp associated to the event
	struct __calqueue_node *next;	// Pointers to other nodes
	unsigned char payload[];	// Actual buffer keeping the payload
} calqueue_node;

extern void calqueue_init(void);
extern void *calqueue_get(void);
extern void calqueue_free(void *ptr);
extern void calqueue_insert(double timestamp, void *payload, size_t size);

#endif				/* __CALQUEUE_H */
