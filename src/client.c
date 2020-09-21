/* client.c: Message Queue Client */

#include "mq/client.h"
#include "mq/logging.h"
#include "mq/socket.h"
#include "mq/string.h"

/* Internal Constants */

#define SENTINEL "SHUTDOWN"

/* Internal Prototypes */

void * mq_pusher(void *);
void * mq_puller(void *);

/* External Functions */

/**
 * Create Message Queue withs specified name, host, and port.
 * @param   name        Name of client's queue.
 * @param   host        Address of server.
 * @param   port        Port of server.
 * @return  Newly allocated Message Queue structure.
 */
MessageQueue * mq_create(const char *name, const char *host, const char *port) {

	MessageQueue *mq = calloc(1, sizeof(MessageQueue));
	if(mq) {
		mq->name = name;
		mq->host = host;
		mq->port = port;
		mq->outgoing = queue_create();
		mq->incoming = queue_create();
		mq->shutdown = false;

		mutex_init(&mq->lock, NULL);
		cond_init(&mq->consumed, NULL);
		cond_init(&mq->produced, NULL);
	}
    return mq;
}

/**
 * Delete Message Queue structure (and internal resources).
 * @param   mq      Message Queue structure.
 */
void mq_delete(MessageQueue *mq) {
	if(mq) {
		queue_delete(mq->outgoing);
		queue_delete(mq->incoming);
		free(mq);
	}
}

/**
 * Publish one message to topic (by placing new Request in outgoing queue).
 * @param   mq      Message Queue structure.
 * @param   topic   Topic to publish to.
 * @param   body    Message body to publish.
 */
void mq_publish(MessageQueue *mq, const char *topic, const char *body) {
	char uri[BUFSIZ];
	sprintf(uri, "/topic/%s", topic);
	Request *r = request_create("PUT", uri, body);
	queue_push(mq->outgoing, r);
}

/**
 * Retrieve one message (by taking Request from incoming queue).
 * @param   mq      Message Queue structure.
 * @return  Newly allocated message body (must be freed).
 */
char * mq_retrieve(MessageQueue *mq) {
	Request *r = queue_pop(mq->incoming);
	char *body = strdup(r->body);
	request_delete(r);
    return body;
}

/**
 * Subscribe to specified topic.
 * @param   mq      Message Queue structure.
 * @param   topic   Topic string to subscribe to.
 **/
void mq_subscribe(MessageQueue *mq, const char *topic) {
	char uri[BUFSIZ];
	sprintf(uri, "/subscription/%s/%s", mq->name, topic);
	Request *r = request_create("PUT", uri, NULL);
	queue_push(mq->outgoing, r);
}

/**
 * Unubscribe to specified topic.
 * @param   mq      Message Queue structure.
 * @param   topic   Topic string to unsubscribe from.
 **/
void mq_unsubscribe(MessageQueue *mq, const char *topic) { 
	char uri[BUFSIZ];
	sprintf(uri, "/subscription/%s/%s", mq->name, topic);
	Request *r = request_create("DELETE", uri, NULL);
	queue_push(mq->outgoing, r);
}

/**
 * Start running the background threads:
 *  1. First thread should continuously send requests from outgoing queue.
 *  2. Second thread should continuously receive reqeusts to incoming queue.
 * @param   mq      Message Queue structure.
 */
void mq_start(MessageQueue *mq) {
	char uri[BUFSIZ];
	sprintf(uri, "/subscription/%s/%s", SENTINEL, SENTINEL);
	Request *r = request_create("PUT", uri, NULL);
	queue_push(mq->outgoing, r);
	thread_create(&mq->puller, NULL, mq_puller, mq);
	thread_create(&mq->pusher, NULL, mq_pusher, mq);
}

/**
 * Stop the message queue client by setting shutdown attribute and sending
 * sentinel messages
 * @param   mq      Message Queue structure.
 */
void mq_stop(MessageQueue *mq) {
	mq->shutdown = true;
	Request *r = (SENTINEL, NULL, NULL);
	queue_push(mq->outgoing, r);
	thread_join(&mq->puller, NULL);
	thread_join(&mq->pusher, NULL);
}

/**
 * Returns whether or not the message queue should be shutdown.
 * @param   mq      Message Queue structure.
 */
bool mq_shutdown(MessageQueue *mq) {
    return mq->shutdown;
}

/* Internal Functions */

/**
 * Pusher thread takes messages from outgoing queue and sends them to server.
 * @param   arg     Message Queue structure.
 **/
void * mq_pusher(void *arg) {
	MessageQueue *mq = (MessageQueue*) arg;
	char buf[BUFSIZ];
	while(!mq_shutdown(mq)) {
		Request *r = queue_pop(mq->outgoing);
		FILE *fs = socket_connect(mq->host, mq->port);
		if(!fs) {
			request_delete(r);
			continue;
		}
		request_write(r, fs);
		fgets(buf, BUFSIZ, fs);
		request_delete(r);
		fclose(fs);
	}
    pthread_exit((void *)0);
}

/**
 * Puller thread requests new messages from server and then puts them in
 * incoming queue.
 * @param   arg     Message Queue structure.
 **/
void * mq_puller(void *arg) {
	MessageQueue *mq = (MessageQueue*) arg;
	while(!mq_shutdown(mq)) {
		Request *r = sizeof(Request);
		FILE *fs = socket_connect(mq->host, mq->port);
		if(!fs) {
			request_delete(r);
			continue;
		}
		request_write(r, fs);
		// read response
		// put request with body into incoming 
		request_delete(r);
		fclose(fs);
	}
    pthread_exit((void *)0);
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
