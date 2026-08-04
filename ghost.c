/**
 * @file ghost.c
 * @brief Behaviour and thread logic for the Ghost entity.
 *
 * Implements initialization, state updates, movement, evidence placement,
 * and thread-controlled autonomous behaviour. All interactions with shared
 * rooms use canonical semaphore locking to ensure thread safety.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h> 
#include "defs.h"
#include "helpers.h"

/**
 * @brief Initializes the Ghost structure.
 * * Selects a random ghost type and a random starting room, and sets the room pointer.
 * @param[in] ghost Pointer to the Ghost structure.
 * @param[in] house Pointer to the House structure to access rooms.
 */
void ghost_init(struct Ghost* ghost, struct House* house) {
    ghost->id = DEFAULT_GHOST_ID;
    
    // Assign a random Ghost Type
    const enum GhostType* ghost_types;
    int ghost_count = get_all_ghost_types(&ghost_types);
    ghost->rng_state = random_derive_seed((unsigned int)ghost->id);
    ghost->type = ghost_types[rand_int_r(&ghost->rng_state, 0, ghost_count)];
    
    // Assign a random starting room
    int random_room = rand_int_r(&ghost->rng_state, 1, house->room_count);
    ghost->current_room = &house->rooms[random_room];
    
    // Set the ghost pointer in the starting room
    ghost->current_room->ghost = ghost;
    
    ghost->boredom = 0;
    atomic_init(&ghost->exited, false);
    ghost->ticks = 0;
    ghost->moves = 0;
    ghost->evidence_dropped = 0;
    ghost->finalized = false;
    
    log_ghost_init(ghost->id, ghost->current_room->name, ghost->type);
}

/**
 * @brief Updates the Ghost's boredom statistic.
 * * Boredom resets if a hunter is present; otherwise, it increments.
 * @param[in] ghost Pointer to the Ghost structure.
 */
void ghost_update_stats(struct Ghost* ghost) {
    pthread_mutex_lock(&ghost->current_room->mutex);
    if (room_has_hunter(ghost->current_room)) {
        ghost->boredom = 0;
    } else {
        ghost->boredom++;
    }
    pthread_mutex_unlock(&ghost->current_room->mutex);
}

/**
 * @brief Checks if the boredom limit is reached, causing the ghost to exit.
 * @param[in] ghost Pointer to the Ghost structure.
 * @return True if the ghost should exit, false otherwise.
 */
bool ghost_check_exit(struct Ghost* ghost) {
    if (ghost->boredom >= ENTITY_BOREDOM_MAX) {
        return true;
    }
    return false;
}

/**
 * @brief Drops one of the ghost's required evidence types in the current room.
 * * Randomly selects one of the 3 required evidence types.
 * @param[in] ghost Pointer to the Ghost structure.
 */
void ghost_leave_evidence(struct Ghost* ghost) {
    EvidenceByte ghost_evidence = (EvidenceByte)ghost->type;
    
    // This stores the bit position 0-6 of the required evidence.
    int evidence_bits[7]; 
    int count = 0;
    for (int i = 0; i < 7; i++) {
        if ((ghost_evidence & (1 << i)) != 0) {
            evidence_bits[count] = i;
            count++;
        }
    }
    
    if (count > 0) {
        // Select one of the required evidence types randomly
        int random_index = rand_int_r(&ghost->rng_state, 0, count);
        enum EvidenceType evidence = (enum EvidenceType)(1 << evidence_bits[random_index]);
        
        pthread_mutex_lock(&ghost->current_room->mutex);
        room_add_evidence(ghost->current_room, evidence); 
        pthread_mutex_unlock(&ghost->current_room->mutex);
        ghost->evidence_dropped++;
        
        log_ghost_evidence(ghost->id, ghost->boredom, 
                          ghost->current_room->name, evidence);
    }
}

/**
 * @brief Moves the ghost to a random connected room if no hunters are present.
 * * Implements canonical locking for deadlock prevention.
 * @param[in] ghost Pointer to the Ghost structure.
 */
void ghost_move(struct Ghost* ghost) {
    if (ghost->current_room->connection_count == 0) {
        return;
    }
    
    // Select a random adjacent room
    int random_index = rand_int_r(&ghost->rng_state, 0, ghost->current_room->connection_count);
    struct Room* next_room = ghost->current_room->connections[random_index];
    
    struct Room* room1 = ghost->current_room;
    struct Room* room2 = next_room;
    
    // Canonical locking order for deadlock prevention (lock by memory address)
    if (room1 > room2) {
        struct Room* temp = room1;
        room1 = room2;
        room2 = temp;
    }
    
    // Acquire locks in canonical order
    pthread_mutex_lock(&room1->mutex);
    pthread_mutex_lock(&room2->mutex);

    // Recheck only after both rooms are locked; a hunter may have entered
    // between choosing the destination and acquiring this lock pair.
    if (room_has_hunter(ghost->current_room)) {
        pthread_mutex_unlock(&room2->mutex);
        pthread_mutex_unlock(&room1->mutex);
        return;
    }
    
    // Update room state
    ghost->current_room->ghost = NULL; // Clear ghost pointer in old room
    next_room->ghost = ghost;          // Set ghost pointer in new room
    
    char from_name[MAX_ROOM_NAME];
    strncpy(from_name, ghost->current_room->name, MAX_ROOM_NAME - 1);
    from_name[MAX_ROOM_NAME - 1] = '\0';
    
    log_ghost_move(ghost->id, ghost->boredom, from_name, next_room->name);
    
    // Final state update
    ghost->current_room = next_room;
    ghost->moves++;
    
    // Release locks
    pthread_mutex_unlock(&room2->mutex);
    pthread_mutex_unlock(&room1->mutex);
}

/**
 * @brief The main thread function for the Ghost.
 * * Runs the Ghost's simulation loop, checking conditions and performing a 
 * random action (move, evidence, or idle).
 * @param[in] arg Pointer to the Ghost structure.
 * @return NULL on thread termination.
 */
bool ghost_step(struct Ghost* ghost) {
    if (atomic_load(&ghost->exited)) return false;
    ghost->ticks++;
    ghost_update_stats(ghost);
        
    if (ghost_check_exit(ghost)) {
        atomic_store(&ghost->exited, true);
        return false;
    }
        
    int action = rand_int_r(&ghost->rng_state, 0, 3);
        
    if (action == 0) {
        log_ghost_idle(ghost->id, ghost->boredom, ghost->current_room->name);
    } else if (action == 1) {
        ghost_leave_evidence(ghost);
    } else {
        ghost_move(ghost);
    }
    return true;
}

void ghost_finalize(struct Ghost* ghost) {
    if (ghost->finalized) return;
    ghost->finalized = true;
    log_ghost_exit(ghost->id, ghost->boredom, ghost->current_room->name);
    atomic_store(&ghost->exited, true);
    pthread_mutex_lock(&ghost->current_room->mutex);
    ghost->current_room->ghost = NULL;
    pthread_mutex_unlock(&ghost->current_room->mutex);
}

void* ghost_thread(void* arg) {
    struct Ghost* ghost = (struct Ghost*)arg;
    while (!atomic_load(&ghost->exited)
           && ghost->ticks < simulation_max_ticks()) {
        simulation_sleep_tick();
        ghost_step(ghost);
    }
    ghost_finalize(ghost);
    
    return NULL;
}
