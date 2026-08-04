/**
 * @file helpers.h
 * @brief Utility functions for logging, conversions, random numbers, and thread-safe operations.
 */

#ifndef HELPERS_H
#define HELPERS_H

#include "defs.h"

/**
 * @brief Return the lowercase token for a device.
 * @param[in] evidence  Evidence type value.
 * @return Static string such as "emf"; "unknown" when out of range.
 */
const char* evidence_to_string(enum EvidenceType evidence);

/**
 * @brief Return the lowercase token for a ghost type.
 * @param[in] ghost Ghost type value.
 * @return Static string such as "goryo"; "unknown" when out of range.
 */
const char* ghost_to_string(enum GhostType ghost);

/**
 * @brief Translate a log reason to text.
 * @param[in] reason Exit reason enum.
 * @return Static string like "bored".
 */
const char* exit_reason_to_string(enum LogReason reason);

/**
 * @brief Expose every evidence device.
 * @param[out] list Optional pointer updated to an array of seven entries.
 * @return Number of items in the returned list.
 */
int get_all_evidence_types(const enum EvidenceType** list);

/**
 * @brief Expose every ghost archetype.
 * @param[out] list Optional pointer updated to an array of ghost types.
 * @return Number of ghost entries in the array.
 */
int get_all_ghost_types(const enum GhostType** list);

/**
 * @brief Checks if the evidence collected matches any valid ghost type.
 * @param[in] collected The collected evidence bitmask.
 * @return True if the evidence matches a known ghost type, false otherwise.
 */
bool is_case_solved(EvidenceByte collected);

/**
 * @brief Thread-safe random integer helper.
 * @param[in] lower_inclusive Minimum value (inclusive).
 * @param[in] upper_exclusive Maximum value (exclusive).
 * @return Random number in [lower_inclusive, upper_exclusive).
 */
int rand_int_threadsafe(int lower_inclusive, int upper_exclusive);
void random_set_seed(unsigned int seed);
unsigned int random_derive_seed(unsigned int stream_id);
int rand_int_r(unsigned int* state, int lower_inclusive, int upper_exclusive);
void simulation_set_runtime(unsigned int tick_ms, unsigned int max_ticks);
void simulation_sleep_tick(void);
unsigned int simulation_max_ticks(void);

/**
 * @brief Append a MOVE entry for a hunter.
 * @param[in] id Hunter identifier.
 * @param[in] boredom Current boredom level.
 * @param[in] fear Current fear level.
 * @param[in] from Room the hunter moved from.
 * @param[in] to Room the hunter moved to.
 * @param[in] device Device being carried.
 */
void log_move(int id, int boredom, int fear, const char* from, const char* to, enum EvidenceType device);

/**
 * @brief Append a GATHER entry for the hunter.
 * @param[in] hunter_id Hunter identifier.
 * @param[in] boredom Current boredom level.
 * @param[in] fear Current fear level.
 * @param[in] room_name Room the hunter is in.
 * @param[in] device Device used for gathering.
 * @param[in] solved True if case solved after gathering.
 */
void log_gather(int hunter_id, int boredom, int fear, 
                const char* room_name, enum EvidenceType device, bool solved);

/**
 * @brief Append an EVIDENCE entry for a hunter.
 * @param[in] id Hunter identifier.
 * @param[in] boredom Current boredom level.
 * @param[in] fear Current fear level.
 * @param[in] room Room where evidence was found.
 * @param[in] device Evidence type found.
 */
void log_evidence(int id, int boredom, int fear, const char* room, enum EvidenceType device);

/**
 * @brief Append a SWAP entry for a hunter.
 * @param[in] id Hunter identifier.
 * @param[in] boredom Current boredom level.
 * @param[in] fear Current fear level.
 * @param[in] old_device Device being swapped out.
 * @param[in] new_device Device being swapped in.
 */
void log_swap(int id, int boredom, int fear, enum EvidenceType old_device, enum EvidenceType new_device);

/**
 * @brief Append an EXIT entry for a hunter.
 * @param[in] id Hunter identifier.
 * @param[in] boredom Current boredom level.
 * @param[in] fear Current fear level.
 * @param[in] room Room the hunter leaves from (should be "Van").
 * @param[in] device Device being carried.
 * @param[in] reason Reason for exiting.
 */
void log_exit(int id, int boredom, int fear, const char* room, enum EvidenceType device, enum LogReason reason);

/**
 * @brief Append a MOVE entry for the ghost.
 * @param[in] id Ghost identifier.
 * @param[in] boredom Current boredom level.
 * @param[in] from Room the ghost moved from.
 * @param[in] to Room the ghost moved to.
 */
void log_ghost_move(int id, int boredom, const char* from, const char* to);

/**
 * @brief Append an EVIDENCE entry for the ghost.
 * @param[in] id Ghost identifier.
 * @param[in] boredom Current boredom level.
 * @param[in] room Room where evidence was dropped.
 * @param[in] evidence Evidence type dropped.
 */
void log_ghost_evidence(int id, int boredom, const char* room, enum EvidenceType evidence);

/**
 * @brief Append an EXIT entry for the ghost.
 * @param[in] id Ghost identifier.
 * @param[in] boredom Current boredom level.
 * @param[in] room Room the ghost leaves from.
 */
void log_ghost_exit(int id, int boredom, const char* room);

/**
 * @brief Append an IDLE entry for the ghost.
 * @param[in] id Ghost identifier.
 * @param[in] boredom Current boredom level.
 * @param[in] room Room the ghost stays in.
 */
void log_ghost_idle(int id, int boredom, const char* room);

/**
 * @brief Append a RETURN entry for the hunter.
 * @param[in] id Hunter identifier.
 * @param[in] boredom Current boredom level.
 * @param[in] fear Current fear level.
 * @param[in] room Room the hunter is currently in.
 * @param[in] device Device being carried.
 * @param[in] heading_home true if beginning the return path, false if just moving on the path.
 */
void log_return_to_van(int id, int boredom, int fear, const char* room, enum EvidenceType device, bool heading_home);

/**
 * @brief Append an INIT entry for a hunter.
 * @param[in] id Hunter identifier.
 * @param[in] room Starting room.
 * @param[in] name Hunter name.
 * @param[in] device Initial device.
 */
void log_hunter_init(int id, const char* room, const char* name, enum EvidenceType device);

/**
 * @brief Append an INIT entry for the ghost.
 * @param[in] id Ghost identifier.
 * @param[in] room Starting room.
 * @param[in] type Ghost type.
 */
void log_ghost_init(int id, const char* room, enum GhostType type);

#endif // HELPERS_H
