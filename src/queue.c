/* queue.c: Concurrent Queue of Requests */

#include "mq/queue.h"

/**
 * Create queue structure.
 * @return  Newly allocated queue structure.
 */
Queue * queue_create() {
    Queue *q = calloc(1, sizeof(Queue));
    
    if (q) {
	    q->size = 0;
		mutex_init(&q->lock, NULL);
		cond_init(&q->cond, NULL);
    }
    return q;
}

/**
 * Delete queue structure.
 * @param   q       Queue structure.
 */
void queue_delete(Queue *q) {
    
    Request *curr = q->head;
    Request *next;
    

    while(curr){
        next = curr->next;
        request_delete(curr);
        curr = next;
    }

    free(q);
    
}

/**
 * Push request to the back of queue.
 * @param   q       Queue structure.
 * @param   r       Request structure.
 */
void queue_push(Queue *q, Request *r) {
    mutex_lock(&q->lock);
    
	if(q->head) {
		if(q->tail) {
			q->tail->next = r;
			q->tail = r;
		} else {
			q->head->next = r;
			q->tail = r;
		}
	} else {
		q->head = r;
		q->tail = r;
	}

    q->size++;

    cond_signal(&q->cond);
    mutex_unlock(&q->lock);
}

/**
 * Pop request to the front of queue (block until there is something to return).
 * @param   q       Queue structure.
 * @return  Request structure.
 */
Request * queue_pop(Queue *q) {
    mutex_lock(&q->lock);
    
    while (q->size == 0) {
        cond_wait(&q->cond, &q->lock);
    }

    Request *temp = q->head;
	if(q->head->next) {
		q->head = q->head->next;
	} else {
		q->head = NULL;
	}
    q->size--;
    
    cond_signal(&q->cond);
    mutex_unlock(&q->lock);

    return temp;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
