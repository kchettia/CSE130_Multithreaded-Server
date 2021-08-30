/*
Name:Krshant Chettiar
This queue is based on my linked list from cse101
*/

#include<stdio.h>
#include<stdlib.h>
#include "queue.h"


//Structs
// private NodeObj type
typedef struct NodeObj{
   void* data;
   struct NodeObj* next;
   struct NodeObj* prev;
} NodeObj;

// private Node type
typedef NodeObj* Node;

// private QueueObj type
typedef struct QueueObj{
   Node front;
   Node back;
   int length;
} QueueObj;

/*
Constructors and Destructors
*/
// newNode()
// Returns reference to new Node object. Initializes next and data fields.
// Private.
Node newNode(void* data){
   Node N = malloc(sizeof(NodeObj));
   N->data = data;
   N->next = NULL;
   N->prev = NULL;
   return(N);
}

// freeNode()
// Frees heap memory pointed to by *pN, sets *pN to NULL.
// Private.
void freeNode(Node* pN){
    if( pN!=NULL && *pN!=NULL ){
      free(*pN);
      *pN = NULL;
   }
}

// Creates and returns a new empty Queue.
Queue newQueue(void){
    Queue Q=NULL;
    Q = malloc(sizeof(QueueObj));
    Q->front =Q->back =NULL;
    Q->length = 0;
    return (Q);

}
// Frees all heap memory associated with *pQ, and sets
 // *pQ to NULL.
void freeQueue(Queue* pQ){

    if(*pQ!=NULL && pQ !=NULL){
       while((*pQ)->length>0){
            dequeue(*pQ);
       }
    free(*pQ);
    *pQ=NULL;
    }
}

// Returns the number of elements in L.
int length(Queue Q){
    if(Q==NULL){
        printf("Queue Error: Uninitialized Queue");
         exit(1);
    }
    else
        return Q->length;
}

// Returns front element of L. Pre: length()>0
void* front(Queue Q){
    if(Q==NULL){
        printf("Queue Error: Uninitialized Queue");
        exit(1);
    }
    else if(length(Q)==0){
        printf("Queue Error: Empty Queue");
        exit(1);
    }

    else
        return (Q->front)->data;
}

// Returns back element of L. Pre: length()>0
void* back(Queue Q){
   if(Q==NULL){
        printf("Queue Error: Uninitialized Queue");
        exit(1);
    }
    else if(length(Q)==0){
        printf("Queue Error: Empty Queue");
        exit(1);
    }
    else
        return (Q->back)->data;
}

// Insert new element into L. If L is non-empty,
// insertion takes place before front element.
void enqueue(Queue Q,void* data){

    if(Q==NULL){
        printf("Queue Error: Uninitialized Queue");
        exit(1);
    }

    Node temp = newNode(data);
    if(length(Q)>0){
        Q->front->prev=temp;
        temp->next=Q->front;
        Q->front = temp;
    }
    else
        (Q->front)=(Q->back) = temp;
    
    (Q->length)++;
}

// Delete the back element. Pre: length()>0
void dequeue(Queue Q){
    if(Q==NULL){
        printf("Queue Error: Uninitialized Queue");
        exit(1);
    }

    else if(length(Q)==1){
        Node temp=Q->back;
        freeNode(&temp);
        Q->front = Q->back = NULL;
        
    }

    else if(length(Q)>1){
        Node temp = Q->back;
        Q->back= Q->back->prev;
        temp->prev->next=NULL;
        freeNode(&temp);
    }
     Q->length--;
}


bool isEmpty(Queue Q){

    if(Q==NULL){
        printf("Queue Error: Uninitialized Queue");
        exit(1);
    }
    if(length(Q)==0){
        return true;
    }

    return false;
}