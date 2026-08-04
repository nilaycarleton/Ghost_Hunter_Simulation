#include <assert.h>
#include <stdio.h>
#include "defs.h"
#include "helpers.h"

static void test_stack(void) {
    struct Room a, b;
    struct RoomStack stack;
    room_init(&a, "A", false, 0);
    room_init(&b, "B", false, 1);
    stack_init(&stack);
    stack_push(&stack, &a);
    stack_push(&stack, &b);
    assert(stack_pop(&stack) == &b);
    assert(stack_pop(&stack) == &a);
    assert(stack_pop(&stack) == NULL);
    room_cleanup(&a);
    room_cleanup(&b);
}

static void test_room_and_evidence(void) {
    struct Room room;
    room_init(&room, "Lab", false, 0);
    for (int i = 0; i < MAX_ROOM_OCCUPANCY + 2; i++) room_add_hunter(&room, i);
    assert(room.hunter_count == MAX_ROOM_OCCUPANCY);
    room_add_evidence(&room, EV_EMF);
    room_add_evidence(&room, EV_ORBS);
    assert(room_has_evidence(&room, EV_EMF));
    room_remove_evidence(&room, EV_EMF);
    assert(!room_has_evidence(&room, EV_EMF));
    assert(room_has_evidence(&room, EV_ORBS));

    struct Room neighbors[MAX_CONNECTIONS + 2];
    for (int i = 0; i < MAX_CONNECTIONS + 2; i++) {
        room_init(&neighbors[i], "Neighbor", false, i + 1);
        room_connect(&room, &neighbors[i]);
    }
    assert(room.connection_count == MAX_CONNECTIONS);
    for (int i = 0; i < MAX_CONNECTIONS + 2; i++) room_cleanup(&neighbors[i]);
    room_cleanup(&room);
}

static void test_bfs_shortest_path(void) {
    struct Room rooms[5];
    for (int i = 0; i < 5; i++) room_init(&rooms[i], "room", i == 0, i);
    room_connect(&rooms[4], &rooms[3]);
    room_connect(&rooms[3], &rooms[2]);
    room_connect(&rooms[2], &rooms[0]);
    room_connect(&rooms[4], &rooms[1]);
    room_connect(&rooms[1], &rooms[0]);
    struct Room* path[MAX_ROOMS] = {0};
    assert(bfs_path_find(&rooms[4], path) == 2);
    assert(path[0] == &rooms[1] && path[1] == &rooms[0]);
    for (int i = 0; i < 5; i++) room_cleanup(&rooms[i]);
}

static void test_seed_streams(void) {
    random_set_seed(42);
    unsigned int a = random_derive_seed(101);
    int first = rand_int_r(&a, 0, 1000);
    random_set_seed(42);
    unsigned int b = random_derive_seed(101);
    assert(first == rand_int_r(&b, 0, 1000));
    assert(is_case_solved(GH_MARE));
    assert(!is_case_solved(EV_EMF));
}

static void test_hunter_gathers_matching_evidence(void) {
    struct House house;
    house_init(&house);
    house_populate_rooms(&house);
    struct Hunter hunter;
    random_set_seed(9);
    hunter_init(&hunter, "Tester", 501, &house.rooms[1], &house.case_file, NAV_BFS);
    hunter.device = EV_EMF;
    room_add_evidence(&house.rooms[1], EV_EMF);
    hunter_gather_evidence(&hunter);
    assert((house.case_file.collected & EV_EMF) != 0);
    assert(!room_has_evidence(&house.rooms[1], EV_EMF));
    assert(hunter.returning_to_van);
    assert(hunter.exit_reason == LR_EVIDENCE);
    stack_cleanup(&hunter.path_stack);
    house_cleanup(&house);
}

int main(void) {
    test_stack();
    test_room_and_evidence();
    test_bfs_shortest_path();
    test_seed_streams();
    test_hunter_gathers_matching_evidence();
    puts("All unit tests passed.");
    return 0;
}
