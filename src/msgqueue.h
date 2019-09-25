#ifndef _MSG_QUEUE_H_
#define _MSG_QUEUE_H_

#include <fcntl.h>
#include <sys/stat.h>
#include <zmq.h>
#include <assert.h>

#define MAX_QUEUE_MSG_SIZE	4096
#define MAX_MESSAGE_SIZE	32768
#define QUANTUM_SLEEP		5000
 
typedef struct
{
	volatile int cancel;
	int pipefd [2];
	char *queue_name;
	void *push;
    void *pull;
	void *context;
} queue_t;

void* msgqueue_init ();
void msgqueue_term ();

void msgqueue_reset(const char *queue_name, int msg_max, long queue_max);
queue_t *msgqueue_create(void *context, const char *queue_name, int msg_max, long queue_max);
void msgqueue_cancel(queue_t *q);
int msgqueue_send(queue_t *q, const char *buffer, int length);
int msgqueue_recv(queue_t *q, char *buffer, int max_size);
void msgqueue_destroy(queue_t *q);

#endif
