#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h> 
#include "defs.h"
#include "helpers.h"


// Bonus 4: BFS pathfinding implementation
/**
 * @brief Computes the shortest path from a starting room to the Van using BFS.
 *
 * This function performs a breadth-first search over the room graph,
 * storing the predecessor of each visited room. Once the Van is found,
 * the path is reconstructed in reverse through the predecessor links.
 *
 * @param[in] start_room   Pointer to the room where the hunter currently is.
 * @param[out] path        Output array used to store each step along the shortest path.
 *                         The array will be populated in order (next step → ... → Van).
 *
 * @return The number of rooms in the computed path, or 0 if no path exists.
 */

int bfs_path_find(struct Room* start_room, struct Room** path) {
    if (start_room->is_exit) return 0; // Already at the van

    struct Room* queue[MAX_ROOMS];
    struct Room* predecessor[MAX_ROOMS];
    bool visited[MAX_ROOMS] = {false};
    int head = 0;
    int tail = 0;
    int path_len = 0;
    struct Room* van_room = NULL;
    
    // Initialize predecessor array that tracks the room used to reach each room.
    for (int i = 0; i < MAX_ROOMS; i++) { predecessor[i] = NULL; }

    queue[tail++] = start_room;
    visited[start_room->index] = true;

    // Standard BFS traversal
    while (head < tail) {
        struct Room* current = queue[head++];

        if (current->is_exit) {
            van_room = current;
            break; // Found the exit (Van)
        }

        // Explore neighbors
        for (int i = 0; i < current->connection_count; i++) {
            struct Room* neighbor = current->connections[i];
            if (!visited[neighbor->index]) {
                visited[neighbor->index] = true;
                // Record the path: 'current' is the predecessor of 'neighbor'
                predecessor[neighbor->index] = current;
                queue[tail++] = neighbor;
            }
        }
    }

    // Reconstruct the path backwards
    if (van_room != NULL) {
        struct Room* current = van_room;
        struct Room* reversed_path[MAX_ROOMS];

        // Trace back from Van to start_room using the predecessor array
        while (current != start_room && current != NULL) {
            reversed_path[path_len++] = current;
            current = predecessor[current->index];
        }
        
        // Reverse path to be in order: next_step -> ... -> Van
        for (int i = 0; i < path_len; i++) {
            path[i] = reversed_path[path_len - 1 - i];
        }

        // Ensure path array is properly terminated
        if (path_len < MAX_ROOMS) {
            path[path_len] = NULL;
        }
        return path_len;
    }

    return 0; // No path found
}

/**
 * @brief Initializes a Hunter structure with starting state and parameters.
 *
 * Sets name, ID, starting room, initial random device, and allocates / resets
 * all bookkeeping values (fear, boredom, exit mode, visit counters, BFS path buffer).
 *
 * @param[out] hunter          Pointer to the Hunter structure being initialized.
 * @param[in]  name            String containing hunter's display name.
 * @param[in]  id              Unique integer identifier.
 * @param[in]  starting_room   Room where the hunter begins (typically the Van).
 * @param[in]  case_file       Shared CaseFile structure for evidence collection.
 * @param[in]  use_bfs_path    True if BFS should be used for return-to-van logic.
 */
void hunter_init(struct Hunter* hunter, const char* name, int id, 
                 struct Room* starting_room, struct CaseFile* case_file,
                 enum NavigationStrategy navigation) {
    strncpy(hunter->name, name, MAX_HUNTER_NAME - 1);
    hunter->name[MAX_HUNTER_NAME - 1] = '\0';
    hunter->id = id;
    hunter->current_room = starting_room;
    hunter->case_file = case_file;
    
    const enum EvidenceType* devices;
    int device_count = get_all_evidence_types(&devices);
    // Initial device selection (still random)
    hunter->rng_state = random_derive_seed((unsigned int)id);
    hunter->device = devices[rand_int_r(&hunter->rng_state, 0, device_count)];
    
    stack_init(&hunter->path_stack);
    hunter->fear = 0;
    hunter->boredom = 0;
    atomic_init(&hunter->exited, false);
    hunter->returning_to_van = false;
    hunter->exit_reason = LR_BORED;
    hunter->ticks = 0;
    hunter->moves = 0;
    hunter->evidence_found = 0;
    hunter->finalized = false;

    hunter->navigation = navigation;
    hunter->bfs_path_length = 0;
    for (int i = 0; i < MAX_ROOMS; i++) {
        hunter->visit_count[i] = 0;     // Initialize visit count for each room
        hunter->bfs_path[i] = NULL;     // Initialize BFS path array
    }
    // Mark starting room as visited 
    hunter->visit_count[starting_room->index]++; 
    
    log_hunter_init(id, starting_room->name, name, hunter->device);
}

/**
 * @brief Updates fear and boredom values based on current room state.
 *
 * Fear increases if a ghost is present. Otherwise boredom increases.
 * Fear resets boredom, while boredom grows only when alone.
 *
 * @param[in,out] hunter Pointer to Hunter whose stats are being updated.
 */

void hunter_update_stats(struct Hunter* hunter) {
    // Check for ghost presence in the current room
    pthread_mutex_lock(&hunter->current_room->mutex);
    if (hunter->current_room->ghost != NULL) {
        hunter->boredom = 0;
        hunter->fear++;
    } else {
        hunter->boredom++;
    }
    pthread_mutex_unlock(&hunter->current_room->mutex);
}

/**
 * @brief Determines whether a hunter should begin returning to the Van.
 *
 * This checks for:
 *   - Fear reaching HUNTER_FEAR_MAX
 *   - Boredom reaching ENTITY_BOREDOM_MAX
 *
 * If triggered, the hunter enters “returning_to_van” mode. When BFS is enabled,
 * the shortest path to the Van is precomputed here.
 *
 * @param[in,out] hunter Pointer to the Hunter.
 *
 * @return true if the hunter is returning to the Van, false otherwise.
 */
bool hunter_check_exit_conditions(struct Hunter* hunter) {
    if (hunter->returning_to_van) {
        return true; 
    }
    
    if (hunter->fear >= HUNTER_FEAR_MAX) {
        hunter->exit_reason = LR_AFRAID;
    } else if (hunter->boredom >= ENTITY_BOREDOM_MAX) {
        hunter->exit_reason = LR_BORED;
    } else {
        return false;
    }
    
    // Exit condition met
    hunter->returning_to_van = true;
    
    // Bonus 4: BFS pathfinding setup
    if (hunter->navigation == NAV_BFS) {
        // Calculate the shortest path to the van and store it
        hunter->bfs_path_length = bfs_path_find(hunter->current_room, hunter->bfs_path);
        stack_clear(&hunter->path_stack); // Ensure stack is empty if using BFS
    }

    log_return_to_van(hunter->id, hunter->boredom, hunter->fear,
                      hunter->current_room->name, hunter->device, true);
    return true;
}

/**
 * @brief Attempts to gather evidence in the current room.
 *
 * Removes evidence atomically under the room mutex, updates the case file,
 * and triggers return-to-van mode if the last required evidence type is obtained.
 *
 * Additionally includes a 10% boredom chance to leave early.
 *
 * @param[in,out] hunter Pointer to the Hunter.
 */

void hunter_gather_evidence(struct Hunter* hunter) {
    enum EvidenceType current_device = hunter->device;
    
    pthread_mutex_lock(&hunter->current_room->mutex);
    bool evidence_found = room_has_evidence(hunter->current_room, current_device);
    
    if (evidence_found) {
        room_remove_evidence(hunter->current_room, current_device);
    }
    pthread_mutex_unlock(&hunter->current_room->mutex);
    
    if (evidence_found) {
        pthread_mutex_lock(&hunter->case_file->mutex);
        hunter->case_file->collected |= current_device;
        hunter->case_file->solved = is_case_solved(hunter->case_file->collected);
        bool solved = hunter->case_file->solved;
        pthread_mutex_unlock(&hunter->case_file->mutex);
        hunter->evidence_found++;
        
        log_gather(hunter->id, hunter->boredom, hunter->fear, 
                   hunter->current_room->name, hunter->device, solved);
        
        // Don't return to van if already there
        if (!hunter->current_room->is_exit) {
            hunter->exit_reason = LR_EVIDENCE; 
            hunter->returning_to_van = true;
            
            if (hunter->navigation == NAV_BFS) {
                hunter->bfs_path_length = bfs_path_find(hunter->current_room, hunter->bfs_path);
                stack_clear(&hunter->path_stack);
            }
            log_return_to_van(hunter->id, hunter->boredom, hunter->fear,
                            hunter->current_room->name, hunter->device, true);
        }
    }
    
    // 10% chance to become bored and return to van (only if not already returning)
    if (!hunter->returning_to_van && !hunter->current_room->is_exit) {
        int random_chance = rand_int_r(&hunter->rng_state, 0, 100);
        if (random_chance < 10) {
            hunter->exit_reason = LR_BORED; 
            hunter->returning_to_van = true;

            if (hunter->navigation == NAV_BFS) {
                hunter->bfs_path_length = bfs_path_find(hunter->current_room, hunter->bfs_path);
                stack_clear(&hunter->path_stack);
            }
            log_return_to_van(hunter->id, hunter->boredom, hunter->fear,
                            hunter->current_room->name, hunter->device, true);
        }
    }
}

/**
 * @brief Handles all logic performed when a hunter is in the Van.
 *
 * If the hunter is returning_to_van, this function finalizes the exit.
 * Otherwise, the hunter performs a device swap prioritizing uncollected
 * evidence types.
 *
 * @param[in,out] hunter Pointer to the Hunter.
 *
 * @return true if the hunter has exited and the thread should terminate.
 */

bool hunter_check_van(struct Hunter* hunter) {
    if (!hunter->current_room->is_exit) {
        return false;
    }
    
    if (atomic_load(&hunter->exited)) {
        return false;
    }
    
    // If returning and inside the van, exit the thread
    if (hunter->returning_to_van) {
        atomic_store(&hunter->exited, true);
        log_return_to_van(hunter->id, hunter->boredom, hunter->fear,
                          hunter->current_room->name, hunter->device, false);
        return true;
    }

    // Normal Van visit
    enum EvidenceType old_device = hunter->device;
    const enum EvidenceType* devices;
    int device_count = get_all_evidence_types(&devices);
    enum EvidenceType new_device = 0;
    
    // Prioritize a device not already collected
    pthread_mutex_lock(&hunter->case_file->mutex);
    EvidenceByte collected = hunter->case_file->collected;
    pthread_mutex_unlock(&hunter->case_file->mutex);

    enum EvidenceType uncollected_devices[7];
    int uncollected_count = 0;

    // Build a list of uncollected evidence types
    for (int i = 0; i < device_count; i++) {
        if (!(collected & devices[i])) {
            uncollected_devices[uncollected_count++] = devices[i];
        }
    }

    if (uncollected_count > 0) {
        // Choosen randomly from the uncollected devices
        new_device = uncollected_devices[rand_int_r(&hunter->rng_state, 0, uncollected_count)];
    } else {
        // All evidence collected which is choosen randomly from all available devices
        new_device = devices[rand_int_r(&hunter->rng_state, 0, device_count)];
    }
    
    hunter->device = new_device;

    if (old_device != hunter->device) {
        log_swap(hunter->id, hunter->boredom, hunter->fear, old_device, hunter->device);
    }
    
    return false;
}


/**
 * @brief Determines and performs the hunter’s next movement step.
 *
 * Exploration mode (not returning):
 *   - Uses “least visited room” heuristic to distribute search effort.
 *   - Pushes breadcrumbs (current room) onto the stack.
 *
 * Return-to-van mode:
 *   - If BFS is enabled, follows the precomputed shortest path.
 *   - If stack mode is used, pops from the breadcrumb stack.
 *   - If the stack is empty but the Van is adjacent, performs the final step.
 *
 * Implements canonical lock ordering (lowest address first)
 * to avoid deadlocks when changing rooms.
 *
 * @param[in,out] hunter Pointer to the Hunter.
 *
 * @return true if movement occurred, false if blocked or no valid move.
 */

bool hunter_move(struct Hunter* hunter) {
    struct Room* next_room = NULL;
    
    if (hunter->returning_to_van) {
        // Pathfinding Logic
        if (hunter->navigation == NAV_BFS) {
            // BFS: Follow the pre-calculated path array
            if (hunter->bfs_path_length > 0) {
                next_room = hunter->bfs_path[0];
            } else {
                // Path exhausted but still not at van
                next_room = stack_pop(&hunter->path_stack);
            }
        } else if (hunter->navigation == NAV_BREADCRUMB) {
            next_room = stack_pop(&hunter->path_stack);

            /* 
            If the stack is exhausted (NULL), the hunter may be in the room
            directly connected to the Van. Check immediate neighbours and
            move into the Van if it is adjacent.
            */
            if (next_room == NULL) {
                for (int i = 0; i < hunter->current_room->connection_count; i++) {
                    struct Room* candidate = hunter->current_room->connections[i];
                    if (candidate->is_exit) {
                        next_room = candidate;
                        break;
                    }
                }
            }
        } else {
            int count = hunter->current_room->connection_count;
            if (count > 0) {
                next_room = hunter->current_room->connections[
                    rand_int_r(&hunter->rng_state, 0, count)];
            }
        }

        if (next_room == NULL || next_room == hunter->current_room) {
            return false; // Path exhausted or stuck
        }

    } else {
        // Least Visited Room
        int min_visits = -1;
        struct Room* preferred_rooms[MAX_CONNECTIONS];
        int preferred_count = 0;
        
        // Iterate through all connected rooms
        for (int i = 0; i < hunter->current_room->connection_count; i++) {
            struct Room* adjacent = hunter->current_room->connections[i];
            int current_visits = hunter->visit_count[adjacent->index];

            // If a room is less visited than the current minimum, reset preference list
            if (min_visits == -1 || current_visits < min_visits) {
                min_visits = current_visits;
                preferred_count = 0; 
                preferred_rooms[preferred_count++] = adjacent;
            } 
            // If a room has the same visit count as the minimum, add it to the preference list
            else if (current_visits == min_visits) {
                preferred_rooms[preferred_count++] = adjacent;
            }
        }

        if (preferred_count > 0) {
            // Choose randomly from the preferred (least visited) rooms
            next_room = preferred_rooms[rand_int_r(&hunter->rng_state, 0, preferred_count)];
        } else {
             return false;
        }
    }
    
    // Canonical Locking Order
    struct Room* room1 = hunter->current_room;
    struct Room* room2 = next_room;
    if (room1 > room2) { struct Room* temp = room1; room1 = room2; room2 = temp; }
    pthread_mutex_lock(&room1->mutex);
    pthread_mutex_lock(&room2->mutex);
    
    bool has_space = room_has_space(next_room);
    
    if (has_space) {
        // Update room state
        room_remove_hunter(hunter->current_room, hunter->id);
        room_add_hunter(next_room, hunter->id);
        
        char from_name[MAX_ROOM_NAME];
        strncpy(from_name, hunter->current_room->name, MAX_ROOM_NAME - 1);
        from_name[MAX_ROOM_NAME - 1] = '\0';
        
        // Log movement 
        log_move(hunter->id, hunter->boredom, hunter->fear, 
                from_name, next_room->name, hunter->device);
        hunter->moves++;
        
        if (!hunter->returning_to_van) {
            // Push to stack and update visit count
            if (!hunter->current_room->is_exit) {
                stack_push(&hunter->path_stack, hunter->current_room);
            }
            hunter->visit_count[next_room->index]++;

        } else if (hunter->navigation == NAV_BFS && hunter->bfs_path_length > 0) {
            // Shift the path array to remove the room just entered
            for (int i = 0; i < hunter->bfs_path_length - 1; i++) {
                hunter->bfs_path[i] = hunter->bfs_path[i + 1];
            }
            hunter->bfs_path_length--;
            
            // Clear the last element to avoid confusion
            if (hunter->bfs_path_length == 0) {
                hunter->bfs_path[0] = NULL;
            }
        }
        
        // Final state update
        hunter->current_room = next_room;
    }
    
    pthread_mutex_unlock(&room2->mutex);
    pthread_mutex_unlock(&room1->mutex);
    
    return has_space;
}

/**
 * @brief Main behavior loop for a hunter thread.
 *
 * Repeatedly performs the following sequence:
 *   1. Sleep briefly (simulate time passing)
 *   2. Update fear/boredom
 *   3. Handle van logic if present in Van
 *   4. Check for exit-condition triggers
 *   5. Attempt to gather evidence (if not returning)
 *   6. Move to the next room
 *
 * The loop ends when the hunter enters and exits the Van.
 *
 * @param[in] arg Pointer to a Hunter structure.
 * @return NULL upon termination.
 */

bool hunter_step(struct Hunter* hunter) {
    if (atomic_load(&hunter->exited)) return false;
    hunter->ticks++;
    hunter_update_stats(hunter);
        
    if (hunter_check_van(hunter)) return false;
        
    hunter_check_exit_conditions(hunter);
        
    if (!hunter->current_room->is_exit && !hunter->returning_to_van) {
        hunter_gather_evidence(hunter);
    }
        
    hunter_move(hunter);
    return !atomic_load(&hunter->exited);
}

void hunter_finalize(struct Hunter* hunter) {
    if (hunter->finalized) return;
    hunter->finalized = true;
    if (!atomic_load(&hunter->exited)) {
        hunter->exit_reason = LR_TIMEOUT;
        log_exit(hunter->id, hunter->boredom, hunter->fear,
                 hunter->current_room->name, hunter->device, LR_TIMEOUT);
    }
    atomic_store(&hunter->exited, true);
    pthread_mutex_lock(&hunter->current_room->mutex);
    room_remove_hunter(hunter->current_room, hunter->id);
    pthread_mutex_unlock(&hunter->current_room->mutex);
    stack_cleanup(&hunter->path_stack);
}

void* hunter_thread(void* arg) {
    struct Hunter* hunter = (struct Hunter*)arg;
    while (!atomic_load(&hunter->exited)
           && hunter->ticks < simulation_max_ticks()) {
        simulation_sleep_tick();
        hunter_step(hunter);
    }
    hunter_finalize(hunter);
    return NULL;
}
