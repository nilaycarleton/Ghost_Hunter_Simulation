#ifndef DEFS_H
#define DEFS_H

#include <stdbool.h>
#include <pthread.h>
#include <stdatomic.h>

#define MAX_ROOM_NAME 64
#define MAX_HUNTER_NAME 64
#define MAX_ROOMS 24
#define MAX_ROOM_OCCUPANCY 8
#define MAX_CONNECTIONS 8
#define ENTITY_BOREDOM_MAX 15
#define HUNTER_FEAR_MAX 15
#define DEFAULT_GHOST_ID 68057

typedef unsigned char EvidenceByte;

enum NavigationStrategy {
    NAV_BFS = 0,
    NAV_BREADCRUMB = 1,
    NAV_RANDOM = 2
};

// Reasons a hunter may terminate the simulation.
enum LogReason {
    LR_EVIDENCE = 0,
    LR_BORED = 1,
    LR_AFRAID = 2,
    LR_TIMEOUT = 3
};

// Single evidence types
enum EvidenceType {
    EV_EMF          = 1 << 0,
    EV_ORBS         = 1 << 1,
    EV_RADIO        = 1 << 2,
    EV_TEMPERATURE  = 1 << 3,
    EV_FINGERPRINTS = 1 << 4,
    EV_WRITING      = 1 << 5,
    EV_INFRARED     = 1 << 6,
};

// Ghost classifications what are defined by their required evidence combinations
enum GhostType {
    GH_POLTERGEIST  = EV_FINGERPRINTS | EV_TEMPERATURE | EV_WRITING,
    GH_THE_MIMIC    = EV_FINGERPRINTS | EV_TEMPERATURE | EV_RADIO,
    GH_HANTU        = EV_FINGERPRINTS | EV_TEMPERATURE | EV_ORBS,
    GH_JINN         = EV_FINGERPRINTS | EV_TEMPERATURE | EV_EMF,
    GH_PHANTOM      = EV_FINGERPRINTS | EV_INFRARED    | EV_RADIO,
    GH_BANSHEE      = EV_FINGERPRINTS | EV_INFRARED    | EV_ORBS,
    GH_GORYO        = EV_FINGERPRINTS | EV_INFRARED    | EV_EMF,
    GH_BULLIES      = EV_FINGERPRINTS | EV_WRITING     | EV_RADIO,
    GH_MYLING       = EV_FINGERPRINTS | EV_WRITING     | EV_EMF,
    GH_OBAKE        = EV_FINGERPRINTS | EV_ORBS        | EV_EMF,
    GH_YUREI        = EV_TEMPERATURE  | EV_INFRARED    | EV_ORBS,
    GH_ONI          = EV_TEMPERATURE  | EV_INFRARED    | EV_EMF,
    GH_MOROI        = EV_TEMPERATURE  | EV_WRITING     | EV_RADIO,
    GH_REVENANT     = EV_TEMPERATURE  | EV_WRITING     | EV_ORBS,
    GH_SHADE        = EV_TEMPERATURE  | EV_WRITING     | EV_EMF,
    GH_ONRYO        = EV_TEMPERATURE  | EV_RADIO       | EV_ORBS,
    GH_THE_TWINS    = EV_TEMPERATURE  | EV_RADIO       | EV_EMF,
    GH_DEOGEN       = EV_INFRARED     | EV_WRITING     | EV_RADIO,
    GH_THAYE        = EV_INFRARED     | EV_WRITING     | EV_ORBS,
    GH_YOKAI        = EV_INFRARED     | EV_RADIO       | EV_ORBS,
    GH_WRAITH       = EV_INFRARED     | EV_RADIO       | EV_EMF,
    GH_RAIJU        = EV_INFRARED     | EV_ORBS        | EV_EMF,
    GH_MARE         = EV_WRITING      | EV_RADIO       | EV_ORBS,
    GH_SPIRIT       = EV_WRITING      | EV_RADIO       | EV_EMF,
};

// Shared case file containing collected evidence 
struct CaseFile {
    EvidenceByte collected;
    bool         solved;
    pthread_mutex_t mutex;
};

struct Ghost;
struct Room;
struct Hunter;

// Node structure used for the hunter breadcrumb stack.
struct RoomNode {
    struct Room* room;
    struct RoomNode* next;
};

// Simple linked-list stack used for reverse navigation
struct RoomStack {
    struct RoomNode* head;
};

/**
 * @brief Structure representing a room in the house. Rooms track connections,
 *        occupant information, contained evidence, and synchronization state.
 */
struct Room {
    char name[MAX_ROOM_NAME];
    int index; // Unique room index which is used for BFS and visit tracking           
    struct Room* connections[MAX_CONNECTIONS];
    int connection_count;
    struct Ghost* ghost;
    int hunter_ids[MAX_ROOM_OCCUPANCY];
    int hunter_count;
    bool is_exit;
    EvidenceByte evidence;
    pthread_mutex_t mutex;
};

// Represents the ghost entity which includes its evidence type and behavior state
struct Ghost {
    int id;
    enum GhostType type;
    struct Room* current_room;
    int boredom;
    atomic_bool exited;
    unsigned int rng_state;
    unsigned int ticks;
    unsigned int moves;
    unsigned int evidence_dropped;
    bool finalized;
};

/**
 * @brief Represents a hunter exploring the house. Includes movement state,
 *        fear/boredom counters, evidence handling, and optional BFS pathfinding.
 */
struct Hunter {
    char name[MAX_HUNTER_NAME];
    int id;
    struct Room* current_room;
    struct CaseFile* case_file;
    enum EvidenceType device;
    struct RoomStack path_stack;
    int fear;
    int boredom;
    enum LogReason exit_reason;
    atomic_bool exited;
    bool returning_to_van;

    // Bonus 3 & 4 fields
    enum NavigationStrategy navigation;
    int visit_count[MAX_ROOMS];        
    struct Room* bfs_path[MAX_ROOMS];  
    int bfs_path_length;               
    unsigned int rng_state;
    unsigned int ticks;
    unsigned int moves;
    unsigned int evidence_found;
    bool finalized;
};

/**
 * @brief Aggregates all simulation state: rooms, hunters, shared case file,
 *        and the ghost instance.
 */
struct House {
    struct Room rooms[MAX_ROOMS];
    int room_count;
    struct Room* starting_room;
    struct Hunter* hunters;
    int hunter_count;
    int hunter_capacity;
    struct CaseFile case_file;
    struct Ghost ghost;
};

// Room functions
void room_init(struct Room* room, const char* name, bool is_exit, int index);
void room_connect(struct Room* a, struct Room* b);
void room_cleanup(struct Room* room);
bool room_has_hunter(struct Room* room);
bool room_has_space(struct Room* room);
void room_add_hunter(struct Room* room, int hunter_id);
void room_remove_hunter(struct Room* room, int hunter_id);
void room_add_evidence(struct Room* room, enum EvidenceType evidence);
bool room_has_evidence(struct Room* room, enum EvidenceType device);
void room_remove_evidence(struct Room* room, enum EvidenceType device);

// House functions
void house_init(struct House* house);
void house_populate_rooms(struct House* house); 
void house_add_hunter(struct House* house, struct Hunter* hunter);
void house_cleanup(struct House* house);

// Hunter functions
void hunter_init(struct Hunter* hunter, const char* name, int id, 
                 struct Room* starting_room, struct CaseFile* case_file,
                 enum NavigationStrategy navigation);
void hunter_update_stats(struct Hunter* hunter);
void hunter_gather_evidence(struct Hunter* hunter);
bool hunter_move(struct Hunter* hunter);
bool hunter_step(struct Hunter* hunter);
void hunter_finalize(struct Hunter* hunter);
void* hunter_thread(void* arg);
int bfs_path_find(struct Room* start_room, struct Room** path);

// Ghost functions
void ghost_init(struct Ghost* ghost, struct House* house);
bool ghost_step(struct Ghost* ghost);
void ghost_finalize(struct Ghost* ghost);
void* ghost_thread(void* arg);

// Stack functions
void stack_init(struct RoomStack* stack);
void stack_push(struct RoomStack* stack, struct Room* room);
struct Room* stack_pop(struct RoomStack* stack);
void stack_clear(struct RoomStack* stack);
void stack_cleanup(struct RoomStack* stack);
bool stack_is_empty(struct RoomStack* stack);

#endif
