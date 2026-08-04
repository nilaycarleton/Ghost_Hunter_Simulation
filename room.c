/**
 * @file room.c
 * @brief Implementation for Room structure initialization and management functions.
 * * Provides thread-safe methods for adding/removing hunters and evidence,
 * and managing room connections.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"

/**
 * @brief Initializes a single Room structure.
 * * Sets the room's name, flags, and initializes its synchronization mutex.
 * @param[in] room Pointer to the Room structure to initialize.
 * @param[in] name The name of the room.
 * @param[in] is_exit True if the room is the Van, false otherwise.
 */
void room_init(struct Room* room, const char* name, bool is_exit, int index) {
    strncpy(room->name, name, MAX_ROOM_NAME - 1);
    room->name[MAX_ROOM_NAME - 1] = '\0';
    room->index = index; //
    room->connection_count = 0;
    room->ghost = NULL;
    room->hunter_count = 0;
    room->is_exit = is_exit;
    room->evidence = 0;
    pthread_mutex_init(&room->mutex, NULL);
}

/**
 * @brief Connects two rooms bidirectionally.
 * @param[in] a Pointer to the first room.
 * @param[in] b Pointer to the second room.
 */
void room_connect(struct Room* a, struct Room* b) {
    if (a->connection_count < MAX_CONNECTIONS) {
        a->connections[a->connection_count] = b;
        a->connection_count++;
    }
    if (b->connection_count < MAX_CONNECTIONS) {
        b->connections[b->connection_count] = a;
        b->connection_count++;
    }
}

/**
 * @brief Cleans up a room structure by destroying its mutex.
 * @param[in] room Pointer to the Room structure to clean up.
 */
void room_cleanup(struct Room* room) {
    pthread_mutex_destroy(&room->mutex);
}

/**
 * @brief Adds a hunter's ID to the room's list.
 * @param[in] room Pointer to the room.
 * @param[in] hunter_id The ID of the hunter to add.
 * * NOTE: Assumes the room's mutex is already held by the caller.
 */
void room_add_hunter(struct Room* room, int hunter_id) {
    if (room->hunter_count < MAX_ROOM_OCCUPANCY) {
        room->hunter_ids[room->hunter_count] = hunter_id;
        room->hunter_count++;
    }
}

/**
 * @brief Removes a hunter's ID from the room's list.
 * @param[in] room Pointer to the room.
 * @param[in] hunter_id The ID of the hunter to remove.
 * * NOTE: Assumes the room's mutex is already held by the caller.
 */
void room_remove_hunter(struct Room* room, int hunter_id) {
    for (int i = 0; i < room->hunter_count; i++) {
        if (room->hunter_ids[i] == hunter_id) {
            // Shift remaining elements to fill the gap
            for (int j = i; j < room->hunter_count - 1; j++) {
                room->hunter_ids[j] = room->hunter_ids[j + 1];
            }
            room->hunter_count--;
            return;
        }
    }
}

/**
 * @brief Checks if there is at least one hunter in the room.
 * @param[in] room Pointer to the room.
 * @return True if hunter_count > 0, false otherwise.
 * * NOTE: Assumes the room's mutex is already held by the caller.
 */
bool room_has_hunter(struct Room* room) {
    return room->hunter_count > 0;
}

/**
 * @brief Checks if there is space for another hunter in the room.
 * @param[in] room Pointer to the room.
 * @return True if hunter_count < MAX_ROOM_OCCUPANCY, false otherwise.
 * * NOTE: Assumes the room's mutex is already held by the caller.
 */
bool room_has_space(struct Room* room) {
    return room->hunter_count < MAX_ROOM_OCCUPANCY;
}

/**
 * @brief Adds a piece of evidence to the room's evidence bit field.
 * @param[in] room Pointer to the room.
 * @param[in] evidence The evidence type to add (as a bit flag).
 * * NOTE: Assumes the room's mutex is already held by the caller.
 */
void room_add_evidence(struct Room* room, enum EvidenceType evidence) {
    room->evidence |= evidence;
}

/**
 * @brief Checks if a specific evidence type is present in the room.
 * @param[in] room Pointer to the room.
 * @param[in] device The evidence type to check for.
 * @return True if the evidence is present, false otherwise.
 * * NOTE: Assumes the room's mutex is already held by the caller.
 */
bool room_has_evidence(struct Room* room, enum EvidenceType device) {
    return (room->evidence & device) != 0;
}

/**
 * @brief Removes a specific piece of evidence from the room.
 * @param[in] room Pointer to the room.
 * @param[in] device The evidence type to remove.
 * * NOTE: Assumes the room's mutex is already held by the caller.
 */
void room_remove_evidence(struct Room* room, enum EvidenceType device) {
    room->evidence &= ~device;
}
