/*
Name:Krshant Chettiar
This queue is based on my linked list from cse101
*/

#ifndef _QUEUE_H_INCLUDE_
#define _QUEUE_H_INCLUDE_
#include<stdio.h>
#include <stdbool.h> 
/*
Exported Type
*/
typedef struct QueueObj* Queue;

/*
Constructors - Destructors
*/
Queue newQueue(void);
void freeQueue(Queue* pL);

/*
Access Functions
*/
bool isEmpty();
void* front(Queue L);
void* back(Queue L);
int length(Queue L);

/*Manipulation Procedures
*/
void enqueue(Queue L,void *data);
void dequeue(Queue L);


#endif
