/**
 * @file house.c
 * @brief Implements creation, storage, and cleanup logic for the House structure.
 *
 * The House encapsulates:
 *   - All rooms in the simulation
 *   - The ghost and hunters
 *   - The shared CaseFile used for evidence tracking
 *
 * This file contains functions for initializing the house, dynamically managing
 * the hunter list, and freeing associated resources before program termination.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"
#include "helpers.h"

/**
 * @brief Initializes the fields and shared resources within a House structure.
 *
 * The function sets counters to zero, clears pointers, initializes the shared
 * CaseFile (including its mutex), and prepares the embedded Ghost structure.
 *
 * @param[out] house Pointer to the House structure being initialized.
 */
void house_init(struct House* house) {
    house->room_count = 0;
    house->starting_room = NULL;
    house->hunters = NULL;
    house->hunter_count = 0;
    house->hunter_capacity = 0;
    
    // Initialize CaseFile fields and its mutex
    house->case_file.collected = 0;
    house->case_file.solved = false;
    sem_init(&house->case_file.mutex, 0, 1);
    
    // Initialize ghost fields
    house->ghost.id = DEFAULT_GHOST_ID;
    house->ghost.current_room = NULL;
    house->ghost.boredom = 0;
    house->ghost.exited = false;
}

/**
 * @brief Adds a hunter to the house’s internal hunter list.
 *
 * This function grows the underlying dynamic array as needed using realloc.
 * The new hunter's structure is copied into the array (the House does not take
 * ownership of the caller’s allocated memory).
 *
 * @param[in,out] house  Pointer to the House receiving the hunter.
 * @param[in]     hunter Pointer to a Hunter whose contents will be copied in.
 */
void house_add_hunter(struct House* house, struct Hunter* hunter) {
    // Check if the current array capacity is reached
    if (house->hunter_count >= house->hunter_capacity) {
        int new_capacity = (house->hunter_capacity == 0) ? 1 : house->hunter_capacity * 2;
        
        // Use realloc to grow the array
        struct Hunter* new_hunters = (struct Hunter*)realloc(house->hunters, 
                                                              new_capacity * sizeof(struct Hunter));
        if (new_hunters == NULL) {
            printf("Memory allocation error\n");
            exit(1);
        }
        
        house->hunters = new_hunters;
        house->hunter_capacity = new_capacity;
    }
    
    // Copy the new hunter into the array
    house->hunters[house->hunter_count] = *hunter;
    house->hunter_count++;
}

/**
 * @brief Releases all dynamically allocated memory and synchronization objects
 *        associated with a House.
 *
 * This cleanup includes:
 *   - Freeing the hunter array if allocated
 *   - Destroying the CaseFile mutex
 *   - Cleaning up all room-associated mutexes via room_cleanup()
 *
 * The House object itself is not freed because it is typically stack-allocated.
 *
 * @param[in,out] house Pointer to the House structure to be cleaned.
 */
void house_cleanup(struct House* house) {
    // Free the dynamically allocated hunter array
    if (house->hunters != NULL) {
        free(house->hunters);
        house->hunters = NULL;
    }
    
    // Destroy the shared case file mutex
    sem_destroy(&house->case_file.mutex);
    
    // Clean up all room mutexes
    for (int i = 0; i < house->room_count; i++) {
        room_cleanup(&house->rooms[i]);
    }
}