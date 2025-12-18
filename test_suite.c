#include <stdio.h>
#include "defs.h"
#include "CuTest.h"

/**
 * @brief Creates a very small house with predictable structure for unit tests.
 */
void setup_test_house(struct House* house);

/**
 * @brief Convenience helper for allocating a hunter starting in a specific room.
 */
struct Hunter create_test_hunter(int id, struct Room* start_room);

// STACK TESTS
/**
 * @brief Verifies basic stack behaviour such as push/pop order,
 *        handling pop on an empty stack, and clearing contents.
 */
void TestStackLIFO(CuTest* tc) {
    /* Test implementation goes here */
}

// ROOM CAPACITY TESTS
/**
 * @brief Ensures rooms enforce connection limits and occupancy limits.
 *        Verifies that exceeding MAX_CONNECTIONS or MAX_ROOM_OCCUPANCY
 *        is handled properly by the room management helpers.
 */
void TestRoomCapacity(CuTest* tc) {
    /* Test implementation goes here */
}

// HUNTER MOVEMENT BEHAVIOUR TESTS
/**
 * @brief Tests the movement heuristic that chooses among adjacent rooms,
 *        verifying that visit counting affects the choice of next room.
 */
void TestImprovedMovement(CuTest* tc) {
    /* Example test idea:
       - create several rooms with equal initial visit counts
       - move the hunter repeatedly
       - confirm that rooms with fewer visits are preferred
    */
}

// BFS PATHFINDING TESTS
/**
 * @brief Verifies that the BFS helper returns the shortest available route 
 *        between two rooms in small, controlled layouts.
 */
void TestBFSPath(CuTest* tc) {
    /* Example test idea:
       - create a chain of rooms and confirm the direct path is chosen
       - build a small branching structure and confirm BFS returns the 
         shortest valid route
    */
}

// EVIDENCE COLLECTION TESTS
/**
 * @brief Confirms that bitwise operations used for storing and checking
 *        evidence types behave correctly with the defined masks.
 */
void TestEvidenceCollection(CuTest* tc) {
    /* Test implementation goes here */
}


// SUITE SETUP
CuSuite* ProjectGetSuite() {
    CuSuite* suite = CuSuiteNew();
    SUITE_ADD_TEST(suite, TestStackLIFO);
    SUITE_ADD_TEST(suite, TestRoomCapacity);
    SUITE_ADD_TEST(suite, TestImprovedMovement);
    SUITE_ADD_TEST(suite, TestBFSPath);
    SUITE_ADD_TEST(suite, TestEvidenceCollection);
    return suite;
}

