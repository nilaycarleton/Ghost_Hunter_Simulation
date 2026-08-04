#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <stdint.h>
#include <inttypes.h>
#include "defs.h"
#include "helpers.h"

// Static mutex for thread-safe logging and random numbers
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static unsigned int simulation_seed = 1;
static unsigned int runtime_tick_ms = 50;
static unsigned int runtime_max_ticks = 1000;
static atomic_ullong event_sequence = 0;

// CORRECTED STATIC STRUCTURES FOR LOGGING

// 1. Define LogEntityType
typedef enum {
    LOG_ENTITY_HUNTER,
    LOG_ENTITY_GHOST,
    LOG_ENTITY_GENERAL
} LogEntityType;

// 2. Define struct LogRecord (Must be before any function that uses it)
struct LogRecord {
    uint64_t timestamp;
    LogEntityType entity_type;
    int entity_id;
    const char* room;
    const char* device;
    int boredom;
    int fear;
    const char* action;
    const char* extra;
};

// 3. Define the internal logging function prototype (Must be after the struct definition)
static void write_log_record(const struct LogRecord* record); 

static void json_escape(const char* input, char* output, size_t capacity) {
    size_t used = 0;
    if (!input) input = "";
    for (const unsigned char* p = (const unsigned char*)input;
         *p && used + 1 < capacity; p++) {
        const char* escape = NULL;
        char unicode[7];
        if (*p == '"') escape = "\\\"";
        else if (*p == '\\') escape = "\\\\";
        else if (*p == '\n') escape = "\\n";
        else if (*p == '\r') escape = "\\r";
        else if (*p == '\t') escape = "\\t";
        else if (*p < 0x20) {
            snprintf(unicode, sizeof(unicode), "\\u%04x", *p);
            escape = unicode;
        }
        if (escape) {
            size_t length = strlen(escape);
            if (used + length >= capacity) break;
            memcpy(output + used, escape, length);
            used += length;
        } else {
            output[used++] = (char)*p;
        }
    }
    output[used] = '\0';
}

// Utility Functions

/**
 * @brief Thread-safe random integer helper.
 */
int rand_int_threadsafe(int lower_inclusive, int upper_exclusive) {
    if (lower_inclusive >= upper_exclusive) {
        return lower_inclusive;
    }
    // Use thread-local storage for better randomness in multi-threaded environments
    unsigned int seed = (unsigned int)time(NULL) ^ (unsigned int)(uintptr_t)pthread_self();
    return (rand_r(&seed) % (upper_exclusive - lower_inclusive)) + lower_inclusive;
}

void random_set_seed(unsigned int seed) {
    simulation_seed = seed ? seed : 1;
    atomic_store(&event_sequence, 0);
}

unsigned int random_derive_seed(unsigned int stream_id) {
    unsigned int value = simulation_seed ^ (stream_id + 0x9e3779b9U);
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    value ^= value >> 16;
    return value ? value : 1;
}

int rand_int_r(unsigned int* state, int lower_inclusive, int upper_exclusive) {
    if (lower_inclusive >= upper_exclusive) return lower_inclusive;
    return (int)(rand_r(state) % (unsigned int)(upper_exclusive - lower_inclusive))
        + lower_inclusive;
}

void simulation_set_runtime(unsigned int tick_ms, unsigned int max_ticks) {
    runtime_tick_ms = tick_ms;
    runtime_max_ticks = max_ticks;
}

void simulation_sleep_tick(void) {
    if (runtime_tick_ms == 0) return;
    struct timespec delay = {
        .tv_sec = runtime_tick_ms / 1000U,
        .tv_nsec = (long)(runtime_tick_ms % 1000U) * 1000000L
    };
    while (nanosleep(&delay, &delay) != 0 && errno == EINTR) {}
}

unsigned int simulation_max_ticks(void) {
    return runtime_max_ticks;
}

/**
 * @brief Checks if the evidence collected matches any valid ghost type.
 */
bool is_case_solved(EvidenceByte collected) {
    const enum GhostType* ghost_types;
    int count = get_all_ghost_types(&ghost_types);
    for (int i = 0; i < count; i++) {
        if (collected == (EvidenceByte)ghost_types[i]) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Expose every evidence device.
 */
int get_all_evidence_types(const enum EvidenceType** list) {
    static const enum EvidenceType evidence_list[] = {
        EV_EMF, EV_ORBS, EV_RADIO, EV_TEMPERATURE, 
        EV_FINGERPRINTS, EV_WRITING, EV_INFRARED
    };
    if (list) {
        *list = evidence_list;
    }
    return sizeof(evidence_list) / sizeof(evidence_list[0]);
}

/**
 * @brief Expose every ghost archetype.
 */
int get_all_ghost_types(const enum GhostType** list) {
    static const enum GhostType ghost_list[] = {
        GH_POLTERGEIST, GH_THE_MIMIC, GH_HANTU, GH_JINN,
        GH_PHANTOM, GH_BANSHEE, GH_YUREI, GH_MARE
    };
    if (list) {
        *list = ghost_list;
    }
    return sizeof(ghost_list) / sizeof(ghost_list[0]);
}

/**
 * @brief Return the lowercase token for a device.
 */
const char* evidence_to_string(enum EvidenceType evidence) {
    if (evidence & EV_EMF) return "emf";
    if (evidence & EV_ORBS) return "orbs";
    if (evidence & EV_RADIO) return "radio";
    if (evidence & EV_TEMPERATURE) return "temp";
    if (evidence & EV_FINGERPRINTS) return "prints";
    if (evidence & EV_WRITING) return "writing";
    if (evidence & EV_INFRARED) return "infrared";
    return "unknown";
}

/**
 * @brief Return the lowercase token for a ghost type.
 */
void house_populate_rooms(struct House* house) {
    // Willow House layout from Phasmaphobia
    house->room_count = 13;

    // PASS THE INDEX HERE: room_init(room, name, is_exit, index)
    room_init(house->rooms+0, "Van", true, 0); 
    room_init(house->rooms+1, "Hallway", false, 1);
    room_init(house->rooms+2, "Master Bedroom", false, 2);
    room_init(house->rooms+3, "Boy's Bedroom", false, 3);
    room_init(house->rooms+4, "Bathroom", false, 4);
    room_init(house->rooms+5, "Basement", false, 5);
    room_init(house->rooms+6, "Basement Hallway", false, 6);
    room_init(house->rooms+7, "Right Storage Room", false, 7);
    room_init(house->rooms+8, "Left Storage Room", false, 8);
    room_init(house->rooms+9, "Kitchen", false, 9);
    room_init(house->rooms+10, "Living Room", false, 10);
    room_init(house->rooms+11, "Garage", false, 11);
    room_init(house->rooms+12, "Utility Room", false, 12);
    
    // REMOVE THE REDUNDANT LOOP THAT SETS THE INDEX.
    // The index is now set inside room_init via the call above.
    /*
    for (int i = 0; i < house->room_count; i++) {
        house->rooms[i].index = i;
    }
    */

    room_connect(house->rooms+0, house->rooms+1);    // Van - Hallway
    room_connect(house->rooms+1, house->rooms+2);    // Hallway - Master Bedroom
    room_connect(house->rooms+1, house->rooms+3);    // Hallway - Boy's Bedroom
    room_connect(house->rooms+1, house->rooms+4);    // Hallway - Bathroom
    room_connect(house->rooms+1, house->rooms+5);    // Hallway - Basement
    room_connect(house->rooms+1, house->rooms+9);    // Hallway - Kitchen

    room_connect(house->rooms+5, house->rooms+6);    // Basement - Basement Hallway
    room_connect(house->rooms+6, house->rooms+7);    // Basement Hallway - Right Storage
    room_connect(house->rooms+6, house->rooms+8);    // Basement Hallway - Left Storage

    room_connect(house->rooms+9, house->rooms+10);   // Kitchen - Living Room
    room_connect(house->rooms+9, house->rooms+11);   // Kitchen - Garage
    room_connect(house->rooms+11, house->rooms+12);  // Garage - Utility Room
    
    house->starting_room = house->rooms; // Van is the starting room
}

// Logging Functions
static void write_log_record(const struct LogRecord* record) {
    pthread_mutex_lock(&log_mutex);
    unsigned long long sequence = atomic_fetch_add(&event_sequence, 1) + 1;
    
    // Get current time in microseconds
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t timestamp_us = (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;

    char filename[64];
    if (record->entity_type == LOG_ENTITY_HUNTER) {
        snprintf(filename, sizeof(filename), "log_%d.csv", record->entity_id);
    } else {
        snprintf(filename, sizeof(filename), "log_ghost.csv");
    }

    FILE* fp = fopen(filename, "a");
    if (fp == NULL) {
        fprintf(stderr, "Error opening log file: %s\n", filename);
        pthread_mutex_unlock(&log_mutex);
        return;
    }

    fprintf(fp, "%llu,%" PRIu64 ",%s,%d,%s,%s,%d,%d,%s,%s\n",
            sequence, timestamp_us,
            record->entity_type == LOG_ENTITY_HUNTER ? "hunter" : "ghost",
            record->entity_id,
            record->room ? record->room : "",
            record->device ? record->device : "",
            record->boredom,
            record->fear,
            record->action,
            record->extra ? record->extra : "");

    fclose(fp);

    if (getenv("GH_JSON_EVENTS") != NULL) {
        char room[6 * MAX_ROOM_NAME + 1];
        char device[96];
        char action[96];
        char extra[6 * MAX_HUNTER_NAME + 128];
        json_escape(record->room, room, sizeof(room));
        json_escape(record->device, device, sizeof(device));
        json_escape(record->action, action, sizeof(action));
        json_escape(record->extra, extra, sizeof(extra));
        printf("EVENT {\"sequence\":%llu,\"timestamp_us\":%" PRIu64 ","
               "\"entity\":\"%s\",\"id\":%d,\"room\":\"%s\","
               "\"device\":\"%s\",\"boredom\":%d,\"fear\":%d,"
               "\"action\":\"%s\",\"extra\":\"%s\"}\n",
               sequence, timestamp_us,
               record->entity_type == LOG_ENTITY_HUNTER ? "hunter" : "ghost",
               record->entity_id, room, device, record->boredom,
               record->fear, action, extra);
        fflush(stdout);
    }
    pthread_mutex_unlock(&log_mutex);
}

void log_hunter_init(int id, const char* room, const char* name, enum EvidenceType device) {
    struct LogRecord record = {
        .entity_type = LOG_ENTITY_HUNTER,
        .entity_id = id,
        .room = room,
        .device = evidence_to_string(device),
        .boredom = 0,
        .fear = 0,
        .action = "INIT",
        .extra = name
    };
    write_log_record(&record);
    printf("Hunter %d (%s) initialized in %s with %s\n",
           id, name, room, evidence_to_string(device));
}

void log_ghost_init(int id, const char* room, enum GhostType type) {
    struct LogRecord record = {
        .entity_type = LOG_ENTITY_GHOST,
        .entity_id = id,
        .room = room,
        .device = NULL,
        .boredom = 0,
        .fear = 0,
        .action = "INIT",
        .extra = ghost_to_string(type)
    };
    write_log_record(&record);
    printf("Ghost %d (%s) initialized in %s\n",
           id, ghost_to_string(type), room);
}

void log_move(int hunter_id, int boredom, int fear, 
              const char* from_room, const char* to_room, enum EvidenceType device) {
    struct LogRecord record = {
        .entity_type = LOG_ENTITY_HUNTER,
        .entity_id = hunter_id,
        .room = from_room,
        .device = evidence_to_string(device),
        .boredom = boredom,
        .fear = fear,
        .action = "MOVE",
        .extra = to_room
    };
    write_log_record(&record);
    printf("Hunter %d using %s moved from %s to %s (bored=%d fear=%d)\n",
           hunter_id, evidence_to_string(device), from_room, to_room, boredom, fear);
}

void log_ghost_move(int ghost_id, int boredom, const char* from_room, const char* to_room) {
    struct LogRecord record = {
        .entity_type = LOG_ENTITY_GHOST,
        .entity_id = ghost_id,
        .room = from_room,
        .device = NULL,
        .boredom = boredom,
        .fear = 0,
        .action = "MOVE",
        .extra = to_room
    };
    write_log_record(&record);
    printf("Ghost %d [bored=%d] MOVE %s -> %s\n",
           ghost_id, boredom, from_room, to_room);
}

void log_gather(int hunter_id, int boredom, int fear, 
                const char* room_name, enum EvidenceType device, bool solved) {
    char extra_info[32];
    snprintf(extra_info, sizeof(extra_info), "SOLVED=%d", solved);
    
    struct LogRecord record = {
        .entity_type = LOG_ENTITY_HUNTER,
        .entity_id = hunter_id,
        .room = room_name,
        .device = evidence_to_string(device),
        .boredom = boredom,
        .fear = fear,
        .action = "GATHER",
        .extra = extra_info
    };
    write_log_record(&record);
    printf("Hunter %d gathered evidence %s in %s (bored=%d fear=%d)\n",
           hunter_id, evidence_to_string(device), room_name, boredom, fear);
}

void log_swap(int hunter_id, int boredom, int fear, 
              enum EvidenceType old_device, enum EvidenceType new_device) {
    char extra_info[64];
    snprintf(extra_info, sizeof(extra_info), "%s -> %s", 
             evidence_to_string(old_device), evidence_to_string(new_device));

    struct LogRecord record = {
        .entity_type = LOG_ENTITY_HUNTER,
        .entity_id = hunter_id,
        .room = "Van",
        .device = evidence_to_string(new_device),
        .boredom = boredom,
        .fear = fear,
        .action = "SWAP",
        .extra = extra_info
    };
    write_log_record(&record);
    printf("Hunter %d swapped devices: %s -> %s (bored=%d fear=%d)\n",
           hunter_id, evidence_to_string(old_device), evidence_to_string(new_device), boredom, fear);
}

void log_return_to_van(int id, int boredom, int fear, const char* room, enum EvidenceType device, bool heading_home) {
    const char* action = heading_home ? "RETURN" : "EXIT";
    // NOTE: The 'extra' field for EXIT logs should reflect the hunter's exit_reason, 
    // but without access to the full Hunter struct here, we use a placeholder or generic string.
    // For consistency with the logic that triggered this log, we'll use "START" for RETURN and empty for EXIT
    const char* extra = heading_home ? "START" : ""; 

    struct LogRecord record = {
        .entity_type = LOG_ENTITY_HUNTER,
        .entity_id = id,
        .room = room,
        .device = evidence_to_string(device),
        .boredom = boredom,
        .fear = fear,
        .action = action,
        .extra = extra
    };

    write_log_record(&record);

    if (heading_home) {
        printf("Hunter %d using %s heading to van from %s (bored=%d fear=%d)\n",
               id, evidence_to_string(device), room, boredom, fear);
    } else {
        // Since we don't have the exit_reason here, the output will default to 'evidence' reason
        // in the final printf based on the previous full output, or we use a more generic text.
        // Assuming the final printf should reflect the actual reason which is available in main.c
        // For logging consistency, the log_return_to_van for EXIT should receive the reason string
        // but since the prototype doesn't include it, we'll keep the output generic for now.
        printf("Hunter %d using %s exited at %s (bored=%d fear=%d)\n",
               id, evidence_to_string(device), room, boredom, fear);
    }
}

void log_exit(int id, int boredom, int fear, const char* room,
              enum EvidenceType device, enum LogReason reason) {
    struct LogRecord record = {
        .entity_type = LOG_ENTITY_HUNTER,
        .entity_id = id,
        .room = room,
        .device = evidence_to_string(device),
        .boredom = boredom,
        .fear = fear,
        .action = "EXIT",
        .extra = exit_reason_to_string(reason)
    };
    write_log_record(&record);
    printf("Hunter %d stopped in %s: %s (bored=%d fear=%d)\n",
           id, room, exit_reason_to_string(reason), boredom, fear);
}

void log_ghost_evidence(int ghost_id, int boredom, const char* room_name, enum EvidenceType evidence) {
    const char* evidence_text = evidence_to_string(evidence);

    struct LogRecord record = {
        .entity_type = LOG_ENTITY_GHOST,
        .entity_id = ghost_id,
        .room = room_name,
        .device = NULL,
        .boredom = boredom,
        .fear = 0,
        .action = "EVIDENCE",
        .extra = evidence_text
    };

    write_log_record(&record);

    printf("Ghost %d [bored=%d] EVIDENCE %s in %s\n",
           ghost_id,
           boredom,
           evidence_text,
           room_name ? room_name : "");
}

void log_ghost_exit(int ghost_id, int boredom, const char* room_name) {
    struct LogRecord record = {
        .entity_type = LOG_ENTITY_GHOST,
        .entity_id = ghost_id,
        .room = room_name,
        .device = NULL,
        .boredom = boredom,
        .fear = 0,
        .action = "EXIT",
        .extra = ""
    };

    write_log_record(&record);

    printf("Ghost %d [bored=%d] EXIT %s\n",
           ghost_id,
           boredom,
           room_name ? room_name : "");
}

void log_ghost_idle(int ghost_id, int boredom, const char* room_name) {
    struct LogRecord record = {
        .entity_type = LOG_ENTITY_GHOST,
        .entity_id = ghost_id,
        .room = room_name,
        .device = NULL,
        .boredom = boredom,
        .fear = 0,
        .action = "IDLE",
        .extra = ""
    };

    write_log_record(&record);

    printf("Ghost %d [bored=%d] IDLE in %s\n",
           ghost_id,
           boredom,
           room_name ? room_name : "");
}

/**
 * @brief Return the lowercase token for a ghost type.
 */
const char* ghost_to_string(enum GhostType ghost) {
    switch (ghost) {
        case GH_POLTERGEIST:
            return "poltergeist";
        case GH_THE_MIMIC:
            return "the_mimic";
        case GH_HANTU:
            return "hantu";
        case GH_JINN:
            return "jinn";
        case GH_PHANTOM:
            return "phantom";
        case GH_BANSHEE:
            return "banshee";
        case GH_GORYO:
            return "goryo";
        case GH_BULLIES:
            return "bullies";
        case GH_MYLING:
            return "myling";
        case GH_OBAKE:
            return "obake";
        case GH_YUREI:
            return "yurei";
        case GH_ONI:
            return "oni";
        case GH_MOROI:
            return "moroi";
        case GH_REVENANT:
            return "revenant";
        case GH_SHADE:
            return "shade";
        case GH_ONRYO:
            return "onryo";
        case GH_THE_TWINS:
            return "the_twins";
        case GH_DEOGEN:
            return "deogen";
        case GH_THAYE:
            return "thaye";
        case GH_YOKAI:
            return "yokai";
        case GH_WRAITH:
            return "wraith";
        case GH_RAIJU:
            return "raiju";
        case GH_MARE:
            return "mare";
        case GH_SPIRIT:
            return "spirit";
        default:
            return "unknown";
    }
}

/**
 * @brief Translate a log reason to text.
 */
const char* exit_reason_to_string(enum LogReason reason) {
    switch (reason) {
        case LR_EVIDENCE:
            return "evidence";
        case LR_BORED:
            return "bored";
        case LR_AFRAID:
            return "afraid";
        case LR_TIMEOUT:
            return "timeout";
        default:
            return "unknown";
    }
}
