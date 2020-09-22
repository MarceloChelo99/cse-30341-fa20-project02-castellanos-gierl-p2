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
		strcpy(mq->name, name);
		strcpy(mq->host, host);
		strcpy(mq->port, port);
		mq->outgoing = queue_create();
		mq->incoming = queue_create();
		mq->shutdown = false;

		mutex_init(&mq->lock, NULL);
		cond_init(&mq->cond, NULL);
        }else {
            return NULL;
        }
    return mq;
}

/**
 * Delete Message Queue structure (and internal resources).
 * @param   mq      Message Queue structure.
 */
void mq_delete(MessageQueue *mq) {
	if(mq) {
		if (mq->outgoing) queue_delete(mq->outgoing);
		if (mq->incoming) queue_delete(mq->incoming);
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
	debug("body %s and sen %s", body, SENTINEL);
	if(streq(body, SENTINEL)) {
		return NULL;
	}
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
	mq_subscribe(mq, SENTINEL);
	thread_create(&mq->puller, NULL, mq_puller, (void *) mq);
	thread_create(&mq->pusher, NULL, mq_pusher, (void *) mq);
}

/**
 * Stop the message queue client by setting shutdown attribute and sending
 * sentinel messages
 * @param   mq      Message Queue structure.
 */
void mq_stop(MessageQueue *mq) {
	mutex_lock(&mq->lock);
	mq->shutdown = true;
	mutex_unlock(&mq->lock);
    mq_publish(mq, SENTINEL, SENTINEL);
	thread_join(mq->puller, NULL);
	thread_join(mq->pusher, NULL);
}

/**
 * Returns whether or not the message queue should be shutdown.
 * @param   mq      Message Queue structure.
 */
bool mq_shutdown(MessageQueue *mq) {
	mutex_lock(&mq->lock);
	bool status = mq->shutdown;
	mutex_unlock(&mq->lock);
	return status;
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
		FILE *fs = socket_connect(mq->host, mq->port);
		if(!fs) {
			continue;
		}
		Request *r = queue_pop(mq->outgoing);
		request_write(r, fs);
		fgets(buf, BUFSIZ, fs);
		request_delete(r);
		fclose(fs);
	}
    //pthread_exit((void *)0);
    return (void*) 0;
}

/**
 * Puller thread requests new messages from server and then puts them in
 * incoming queue.
 * @param   arg     Message Queue structure.
 **/
void * mq_puller(void *arg) {
	MessageQueue *mq = (MessageQueue*) arg;
			
	char uri[BUFSIZ];
	sprintf(uri, "/queue/%s", mq->name);

	char buf[BUFSIZ];
	size_t length = 0;
	while(!mq_shutdown(mq)) {
		FILE *fs = socket_connect(mq->host, mq->port);
		if(!fs) {
			continue;
		}
		Request *r = request_create("GET", uri, NULL);
		request_write(r, fs);
		fgets(buf, BUFSIZ, fs);
		if(strstr(buf, "200 OK")) {
			while(fgets(buf, BUFSIZ, fs) && !streq(buf, "\r\n")) {
				sscanf(buf, "Content-Length: %ld", &length);
			}
			if(length > 0) {
				r->body = calloc(length + 1, sizeof(char));
				fread(r->body, 1, length, fs);
				if(r->body) queue_push(mq->incoming, r);
				else request_delete(r);
			}
		}
		fclose(fs);
	}
    //pthread_exit((void *)0);
    return (void *) 0;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
