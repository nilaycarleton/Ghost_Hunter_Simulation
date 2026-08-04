#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "defs.h"
#include "helpers.h"

struct Options {
    unsigned int seed;
    enum NavigationStrategy navigation;
    const char* hunters;
    unsigned int tick_ms;
    unsigned int max_ticks;
    bool deterministic;
    const char* output_json;
};

static void cleanup_unstarted_hunters(struct House* house, int first);

static void usage(const char* program) {
    printf("Usage: %s [--seed N] [--navigation bfs|breadcrumb|random] "
           "[--hunters NAME,NAME,...] [--tick-ms 0..10000] "
           "[--max-ticks 1..1000000] [--deterministic] "
           "[--output-json FILE]\n", program);
}

static const char* navigation_name(enum NavigationStrategy navigation) {
    if (navigation == NAV_BFS) return "bfs";
    if (navigation == NAV_BREADCRUMB) return "breadcrumb";
    return "random";
}

static bool parse_uint(const char* text, unsigned long maximum, unsigned int* out) {
    char* end = NULL;
    errno = 0;
    unsigned long value = strtoul(text, &end, 10);
    if (errno || text == end || *end != '\0' || value > maximum) return false;
    *out = (unsigned int)value;
    return true;
}

static bool parse_options(int argc, char** argv, struct Options* options) {
    options->seed = (unsigned int)time(NULL);
    options->navigation = NAV_BFS;
    options->hunters = "Ada,Linus,Grace";
    options->tick_ms = 50;
    options->max_ticks = 1000;
    options->deterministic = false;
    options->output_json = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            if (!parse_uint(argv[++i], 0xffffffffUL, &options->seed)) return false;
        } else if (strcmp(argv[i], "--navigation") == 0 && i + 1 < argc) {
            const char* value = argv[++i];
            if (strcmp(value, "bfs") == 0) options->navigation = NAV_BFS;
            else if (strcmp(value, "breadcrumb") == 0) options->navigation = NAV_BREADCRUMB;
            else if (strcmp(value, "random") == 0) options->navigation = NAV_RANDOM;
            else return false;
        } else if (strcmp(argv[i], "--hunters") == 0 && i + 1 < argc) {
            options->hunters = argv[++i];
        } else if (strcmp(argv[i], "--tick-ms") == 0 && i + 1 < argc) {
            if (!parse_uint(argv[++i], 10000, &options->tick_ms)) return false;
        } else if (strcmp(argv[i], "--max-ticks") == 0 && i + 1 < argc) {
            if (!parse_uint(argv[++i], 1000000, &options->max_ticks)
                || options->max_ticks == 0) return false;
        } else if (strcmp(argv[i], "--deterministic") == 0) {
            options->deterministic = true;
        } else if (strcmp(argv[i], "--output-json") == 0 && i + 1 < argc) {
            options->output_json = argv[++i];
            if (*options->output_json == '\0') return false;
        } else if (strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            exit(0);
        } else {
            return false;
        }
    }
    return true;
}

static bool add_hunters(struct House* house, const char* names,
                        enum NavigationStrategy navigation) {
    size_t names_length = names ? strlen(names) : 0;
    if (names_length == 0 || names[0] == ',' || names[names_length - 1] == ','
        || strstr(names, ",,") != NULL) return false;
    char* copy = strdup(names);
    if (!copy) return false;
    char* save = NULL;
    int id = 101;

    for (char* name = strtok_r(copy, ",", &save); name;
         name = strtok_r(NULL, ",", &save)) {
        while (*name == ' ' || *name == '\t') name++;
        char* end = name + strlen(name);
        while (end > name && (end[-1] == ' ' || end[-1] == '\t')) *--end = '\0';
        bool unsafe = false;
        for (const unsigned char* p = (const unsigned char*)name; *p; p++) {
            if (*p < 0x20 || *p == '"' || *p == '\\') unsafe = true;
        }
        if (*name == '\0' || strlen(name) >= MAX_HUNTER_NAME || unsafe
            || house->hunter_count >= MAX_ROOM_OCCUPANCY) {
            free(copy);
            return false;
        }
        struct Hunter hunter;
        hunter_init(&hunter, name, id++, house->starting_room,
                    &house->case_file, navigation);
        house_add_hunter(house, &hunter);
        room_add_hunter(house->starting_room, hunter.id);
    }
    free(copy);
    return house->hunter_count > 0;
}

static void run_deterministic(struct House* house, unsigned int max_ticks) {
    for (unsigned int tick = 0; tick < max_ticks; tick++) {
        bool active = false;
        simulation_sleep_tick();
        if (!atomic_load(&house->ghost.exited)) {
            active = true;
            if (!ghost_step(&house->ghost)) ghost_finalize(&house->ghost);
        }
        for (int i = 0; i < house->hunter_count; i++) {
            if (!atomic_load(&house->hunters[i].exited)) {
                active = true;
                if (!hunter_step(&house->hunters[i])) {
                    hunter_finalize(&house->hunters[i]);
                }
            }
        }
        if (!active) break;
    }
    ghost_finalize(&house->ghost);
    for (int i = 0; i < house->hunter_count; i++) hunter_finalize(&house->hunters[i]);
}

static bool run_threaded(struct House* house) {
    pthread_t ghost_tid;
    pthread_t hunter_tids[MAX_ROOM_OCCUPANCY];
    int error = pthread_create(&ghost_tid, NULL, ghost_thread, &house->ghost);
    if (error != 0) {
        fprintf(stderr, "pthread_create ghost: %s\n", strerror(error));
        cleanup_unstarted_hunters(house, 0);
        return false;
    }
    int started = 0;
    for (; started < house->hunter_count; started++) {
        error = pthread_create(&hunter_tids[started], NULL, hunter_thread,
                               &house->hunters[started]);
        if (error != 0) {
            fprintf(stderr, "pthread_create hunter: %s\n", strerror(error));
            break;
        }
    }
    if (started != house->hunter_count) {
        atomic_store(&house->ghost.exited, true);
        for (int i = 0; i < started; i++) atomic_store(&house->hunters[i].exited, true);
    }
    pthread_join(ghost_tid, NULL);
    for (int i = 0; i < started; i++) pthread_join(hunter_tids[i], NULL);
    cleanup_unstarted_hunters(house, started);
    return started == house->hunter_count;
}

static bool write_summary(const char* path, const struct Options* options,
                          const struct House* house) {
    if (!path) return true;
    FILE* output = fopen(path, "w");
    if (!output) {
        fprintf(stderr, "Cannot write summary '%s': %s\n", path, strerror(errno));
        return false;
    }
    unsigned int total_moves = house->ghost.moves;
    for (int i = 0; i < house->hunter_count; i++) total_moves += house->hunters[i].moves;
    fprintf(output,
            "{\n  \"seed\": %u,\n  \"scheduler\": \"%s\",\n"
            "  \"navigation\": \"%s\",\n  \"tick_ms\": %u,\n"
            "  \"max_ticks\": %u,\n  \"ghost_type\": \"%s\",\n"
            "  \"solved\": %s,\n  \"evidence_mask\": %u,\n"
            "  \"total_moves\": %u,\n  \"ghost\": {\"ticks\": %u,"
            " \"moves\": %u, \"evidence_dropped\": %u},\n  \"hunters\": [\n",
            options->seed, options->deterministic ? "deterministic" : "threads",
            navigation_name(options->navigation), options->tick_ms,
            options->max_ticks, ghost_to_string(house->ghost.type),
            house->case_file.solved ? "true" : "false",
            house->case_file.collected, total_moves, house->ghost.ticks,
            house->ghost.moves, house->ghost.evidence_dropped);
    for (int i = 0; i < house->hunter_count; i++) {
        const struct Hunter* hunter = &house->hunters[i];
        fprintf(output,
                "    {\"id\": %d, \"name\": \"%s\", \"ticks\": %u,"
                " \"moves\": %u, \"evidence_found\": %u, \"exit\": \"%s\"}%s\n",
                hunter->id, hunter->name, hunter->ticks, hunter->moves,
                hunter->evidence_found, exit_reason_to_string(hunter->exit_reason),
                i + 1 == house->hunter_count ? "" : ",");
    }
    fprintf(output, "  ]\n}\n");
    bool okay = fclose(output) == 0;
    if (!okay) fprintf(stderr, "Failed closing summary '%s'.\n", path);
    return okay;
}

static void cleanup_unstarted_hunters(struct House* house, int first) {
    for (int i = first; i < house->hunter_count; i++) {
        stack_cleanup(&house->hunters[i].path_stack);
    }
}

int main(int argc, char** argv) {
    struct Options options;
    if (!parse_options(argc, argv, &options)) {
        usage(argv[0]);
        return 2;
    }
    random_set_seed(options.seed);
    simulation_set_runtime(options.tick_ms, options.max_ticks);
    printf("Simulation seed: %u | scheduler: %s | navigation: %s | tick: %ums | limit: %u\n",
           options.seed, options.deterministic ? "deterministic" : "threads",
           navigation_name(options.navigation), options.tick_ms, options.max_ticks);

    struct House house;
    house_init(&house);
    house_populate_rooms(&house);
    if (!add_hunters(&house, options.hunters, options.navigation)) {
        fprintf(stderr, "Hunters must be 1-%d safe characters; maximum %d hunters.\n",
                MAX_HUNTER_NAME - 1, MAX_ROOM_OCCUPANCY);
        house_cleanup(&house);
        return 2;
    }
    ghost_init(&house.ghost, &house);

    bool run_okay = true;
    if (options.deterministic) run_deterministic(&house, options.max_ticks);
    else run_okay = run_threaded(&house);

    printf("\n=== SIMULATION RESULTS ===\nGhost Type: %s\nHunter Exit Reasons:\n",
           ghost_to_string(house.ghost.type));
    for (int i = 0; i < house.hunter_count; i++) {
        printf("  %s (ID %d): %s\n", house.hunters[i].name,
               house.hunters[i].id,
               exit_reason_to_string(house.hunters[i].exit_reason));
    }
    printf("Evidence mask: 0x%02x | solved: %s\n", house.case_file.collected,
           house.case_file.solved ? "yes" : "no");
    bool summary_okay = write_summary(options.output_json, &options, &house);
    house_cleanup(&house);
    return run_okay && summary_okay ? 0 : 1;
}
