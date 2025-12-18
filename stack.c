/**
 * @file stack.c
 * @brief Simple linked-list based stack implementation for managing a Hunter's path (breadcrumbs).
 * * Stores pointers to Room structures in a Last-In, First-Out (LIFO) manner.
 */

#include <stdio.h>
#include <stdlib.h>
#include "defs.h"

/**
 * @brief Initializes the stack by setting the head pointer to NULL.
 * @param[in] stack Pointer to the RoomStack to initialize.
 */
void stack_init(struct RoomStack* stack) {
    stack->head = NULL;
}

/**
 * @brief Pushes a Room pointer onto the top of the stack.
 * @param[in] stack Pointer to the RoomStack.
 * @param[in] room Pointer to the Room to be stored.
 */
void stack_push(struct RoomStack* stack, struct Room* room) {
    // Allocate memory for the new node
    struct RoomNode* new_node = (struct RoomNode*)malloc(sizeof(struct RoomNode));
    if (new_node == NULL) {
        printf("Memory allocation error\n");
        exit(1);
    }
    
    // Set node data and link it to the current head
    new_node->room = room;
    new_node->next = stack->head;
    
    // Update the stack head
    stack->head = new_node;
}

/**
 * @brief Pops and returns the Room pointer from the top of the stack.
 * @param[in] stack Pointer to the RoomStack.
 * @return The Room pointer that was at the top, or NULL if the stack is empty.
 */
struct Room* stack_pop(struct RoomStack* stack) {
    if (stack->head == NULL) {
        return NULL;
    }
    
    // Store head, update head, retrieve data, and free old node
    struct RoomNode* temp = stack->head;
    struct Room* room = temp->room;
    stack->head = temp->next;
    free(temp);
    
    return room;
}

/**
 * @brief Checks if the stack is empty.
 * @param[in] stack Pointer to the RoomStack.
 * @return True if the head is NULL, false otherwise.
 */
bool stack_is_empty(struct RoomStack* stack) {
    return stack->head == NULL;
}

/**
 * @brief Clears all elements from the stack, freeing memory.
 * @param[in] stack Pointer to the RoomStack.
 */
void stack_clear(struct RoomStack* stack) {
    while (!stack_is_empty(stack)) {
        stack_pop(stack); // Repeatedly pop to free memory
    }
}

/**
 * @brief Cleanup wrapper (same as stack_clear).
 * @param[in] stack Pointer to the RoomStack.
 */
void stack_cleanup(struct RoomStack* stack) {
    stack_clear(stack);
}