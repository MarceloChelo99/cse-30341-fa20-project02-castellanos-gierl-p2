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
    
    for(Request *ptr = q->head; ptr; ptr = ptr->next){
		request_delete(ptr);
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
    
    if(q->tail != NULL){
        q->tail->next = r;
    }    
    
    q->tail = r;
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
    q->head = q->head->next;
    q->size--;
    
    cond_signal(&q->cond);
    mutex_unlock(&q->lock);

    return temp;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
