#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

// Function to create an empty queue
Queue* createQueue() {
    Queue* queue = (Queue*)malloc(sizeof(Queue));
    queue->front = queue->rear = NULL;
    return queue;
}

// Function to check if the queue is empty
int isQueueEmpty(Queue* queue) {
    return queue->front == NULL;
}

// Function to enqueue an element
void enqueue(Queue* queue, int data) {
    // Create a new node
    Node* newNode = (Node*)malloc(sizeof(Node));
    newNode->data = data;
    newNode->next = NULL;

    // If the queue is empty, make the new node both front and rear
    if (isQueueEmpty(queue)) {
        queue->front = queue->rear = newNode;
    } else {
        // Add the new node at the end and update the rear
        queue->rear->next = newNode;
        queue->rear = newNode;
    }

    printf("Enqueued element: %d\n", data);
}

// Function to dequeue an element
int dequeue(Queue* queue) {
    // If the queue is empty, return -1
    if (isQueueEmpty(queue)) {
        printf("Queue is empty\n");
        return -1;
    }

    // Store the data of the front node
    int data = queue->front->data;

    // Move the front pointer to the next node
    Node* temp = queue->front;
    queue->front = queue->front->next;

    // If the front becomes NULL, update the rear to NULL as well
    if (queue->front == NULL) {
        queue->rear = NULL;
    }

    // Free the memory of the dequeued node
    free(temp);

    return data;
}

// Function to display the elements in the queue
void displayQueue(Queue* queue) {
    if (isQueueEmpty(queue)) {
        printf("Queue is empty\n");
        return;
    }

    printf("Queue elements: ");
    Node* current = queue->front;
    while (current != NULL) {
        printf("%d ", current->data);
        current = current->next;
    }
    printf("\n");
}

// Function to free the memory allocated for the queue
void destroyQueue(Queue* queue) {
    while (!isQueueEmpty(queue)) {
        dequeue(queue);
    }
    free(queue);
}

//Delete a node 

void deleteNode(Queue* queue, int value) {
    // If the queue is empty, return
    if (isQueueEmpty(queue)) {
        printf("Queue is empty\n");
        return;
    }

    // If the node to be deleted is the front node
    if (queue->front->data == value) {
        Node* temp = queue->front;
        queue->front = queue->front->next;
        free(temp);

        // If the front becomes NULL, update the rear to NULL as well
        if (queue->front == NULL) {
            queue->rear = NULL;
        }

        printf("Deleted node with value: %d\n", value);
        return;
    }

    // Find the node to be deleted
    Node* prev = queue->front;
    Node* current = queue->front->next;
    while (current != NULL && current->data != value) {
        prev = current;
        current = current->next;
    }

    // If the node to be deleted is found
    if (current != NULL) {
        prev->next = current->next;

        // If the node to be deleted is the rear node, update the rear
        if (current == queue->rear) {
            queue->rear = prev;
        }

        free(current);
        printf("Deleted node with value: %d\n", value);
    } else {
        printf("Node with value %d not found\n", value);
    }
}


