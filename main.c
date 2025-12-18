#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h> // Include for thread functions
#include "defs.h"
#include "helpers.h"

int main() {
    // Seed random number generator for device selection, movement randomness, etc.
    srand(time(NULL));
    
    // Simulation Setup
    struct House house;
    house_init(&house);
    house_populate_rooms(&house);

    // Bouns 4: Pathfinding toggle input
    int bfs_input = 0;
    printf("Enable BFS Pathfinding for Hunter return? (1=Yes, 0=No/Default Stack): ");
    if (scanf("%d", &bfs_input) != 1) {
        bfs_input = 0; 
    }
    while (getchar() != '\n'); // Clear trailing characters from input buffer
    bool use_bfs_toggle = (bfs_input == 1);

    // Hunter Input Loop
    printf("Enter hunter names (type 'done' when finished):\n");
    
    int hunter_id = 0;
    while (1) {
        char name[MAX_HUNTER_NAME];
        printf("Hunter name: ");

        // Read hunter name or break on EOF/input error
        if (scanf("%63s", name) != 1) {
            break; // Handle EOF or input error
        }
        
        if (strcmp(name, "done") == 0) {
            break; // Exit loop on "done"
        }
        
        printf("Hunter ID: ");
        if (scanf("%d", &hunter_id) != 1) {
            break; // Handle input error
        }
        
        while (getchar() != '\n'); // Clear leftover characters
        
        // Initialize and store hunter
        struct Hunter hunter;
        hunter_init(&hunter, name, hunter_id, house.starting_room, &house.case_file, use_bfs_toggle);
        house_add_hunter(&house, &hunter);
    }
    
    // If no hunters were added, exit cleanly
    if (house.hunter_count == 0) {
        printf("No hunters added. Exiting.\n");
        house_cleanup(&house);
        return 0;
    }
    
    // Initialize ghost after hunters are established
    ghost_init(&house.ghost, &house);
    
    // Thread Creation
    pthread_t ghost_tid;
    // The ghost thread starts running immediately
    if (pthread_create(&ghost_tid, NULL, ghost_thread, &house.ghost) != 0) {
        printf("Error creating ghost thread.\n");
        house_cleanup(&house);
        return 1;
    }
    
    // Create an array for hunter thread IDs
    pthread_t hunter_tids[house.hunter_count];
    for (int i = 0; i < house.hunter_count; i++) {
        // Hunter threads start running immediately
        if (pthread_create(&hunter_tids[i], NULL, hunter_thread, &house.hunters[i]) != 0) {
            printf("Error creating hunter thread %d.\n", house.hunters[i].id);
            // Must clean up partially created threads/resources
            house_cleanup(&house); 
            return 1;
        }
    }
    
    // Thread Synchronization: Joining
    
    // Ghost terminates when bored or when leaving the house
    pthread_join(ghost_tid, NULL);
    
    // Hunters terminate when their exit conditions are reached
    for (int i = 0; i < house.hunter_count; i++) {
        pthread_join(hunter_tids[i], NULL);
    }
    
    // Simulation Results
    printf("\n=== SIMULATION RESULTS ===\n");
    printf("Ghost Type: %s\n", ghost_to_string(house.ghost.type));
    
    // Print hunter's exit reasons
    printf("\nHunter Exit Reasons:\n");
    for (int i = 0; i < house.hunter_count; i++) {
        printf("  %s (ID %d): %s\n", 
               house.hunters[i].name, 
               house.hunters[i].id,
               exit_reason_to_string(house.hunters[i].exit_reason));
    }
    
    // Print the evidence collected
    printf("\nEvidence Collected: ");
    EvidenceByte collected = house.case_file.collected;
    int evidence_count = 0;
    for (int i = 0; i < 7; i++) {
        // Check if the bit for this evidence type is set
        if ((collected & (1 << i)) != 0) {
            if (evidence_count > 0) printf(", ");
            printf("%s", evidence_to_string((enum EvidenceType)(1 << i)));
            evidence_count++;
        }
    }
    if (evidence_count == 0) {
        printf("None");
    }
    printf("\n");
    
    // Check if collected evidence fully matches the ghost type
    if ((house.ghost.type & collected) == house.ghost.type) {
        printf("Evidence matches ghost type: Yes\n");
    } else {
        printf("Evidence does not match a valid ghost type\n");
    }
    
    // Cleanup
    house_cleanup(&house);
    
    return 0;
}