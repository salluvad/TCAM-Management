#ifndef QUEUE_H
#define QUEUE_H

// Queue node structure
typedef struct Node {
    int data;
    struct Node* next;
} Node;

// Queue structure
typedef struct Queue {
    Node* front;
    Node* rear;
} Queue;

// Function to create an empty queue
Queue* createQueue();

// Function to check if the queue is empty
int isQueueEmpty(Queue* queue);

// Function to enqueue an element
void enqueue(Queue* queue, int data);

// Function to dequeue an element
int dequeue(Queue* queue);

// Function to display the elements in the queue
void displayQueue(Queue* queue);

// Function to free the memory allocated for the queue
void destroyQueue(Queue* queue);

// Fucntion to Delete a node 
void deleteNode(Queue* queue, int value);
#endif
