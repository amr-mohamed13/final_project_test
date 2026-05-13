/**
 * dispatcher.c — Task Allocation Algorithm (Master Only)
 *
 * Priority rules for hallway call assignment:
 *   1. IMMEDIATE:    Elevator is IDLE and already AT the calling floor.
 *   2. PERFECT_MATCH: Elevator is moving TOWARD the floor in the SAME direction.
 *   3. PASSED_MATCH:  Same direction but has already PASSED the floor.
 *   4. OPPOSITE_DIR:  Do NOT assign until current path is finished.
 *   5. IDLE_NEAREST:  No directional match → assign to nearest IDLE elevator.
 */

#include "dispatcher.h"

/* ------------------------------------------------------------------ */
/*  Internal state                                                    */
/* ------------------------------------------------------------------ */
static ElevatorState_t *s_elevA;
static ElevatorState_t *s_elevB;

/* Pending hall calls not yet assigned */
static volatile uint8 s_hallUpCalls;     /* Bitmask: bit N = floor N wants UP   */
static volatile uint8 s_hallDownCalls;   /* Bitmask: bit N = floor N wants DOWN */

/* Requests assigned to Slave (sent via SPI) */
static uint8 s_slaveUpAssign;
static uint8 s_slaveDownAssign;

/* ------------------------------------------------------------------ */
/*  Scoring                                                           */
/* ------------------------------------------------------------------ */
typedef enum {
    SCORE_IMMEDIATE     = 0,    /* Best */
    SCORE_PERFECT_MATCH = 1,
    SCORE_IDLE_NEAREST  = 2,
    SCORE_PASSED_MATCH  = 3,
    SCORE_OPPOSITE_DIR  = 4,
    SCORE_UNAVAILABLE   = 0xFF  /* Worst — do not assign */
} DispatchScore_t;

static uint8 AbsDiff(uint8 a, uint8 b) {
    return (a > b) ? (a - b) : (b - a);
}

/**
 * Compute a score for assigning a hall call to an elevator.
 * Lower score = better match.
 */
static uint8 ScoreElevator(const ElevatorState_t *e,
                           uint8 callFloor, uint8 callDir) {
    uint8 estate = e->fsmState;

    /* Emergency — unavailable */
    if (estate == ELEV_EMERGENCY) {
        return SCORE_UNAVAILABLE;
    }

    /* DOORS_OPEN is treated like IDLE at the current floor */
    if (estate == ELEV_IDLE || estate == ELEV_DOORS_OPEN) {
        if (e->currentFloor == callFloor) {
            return SCORE_IMMEDIATE;     /* Rule 1 */
        }
        return SCORE_IDLE_NEAREST;      /* Rule 5 */
    }

    /* Moving elevators */
    uint8 movingUp = (estate == ELEV_MOVING_UP) ? 1U : 0U;
    uint8 elevDir  = movingUp ? DIR_UP : DIR_DOWN;

    if (elevDir == callDir) {
        /* Same direction as the call */
        if (movingUp && callFloor >= e->currentFloor) {
            return SCORE_PERFECT_MATCH; /* Rule 2: moving up, floor is above */
        }
        if (!movingUp && callFloor <= e->currentFloor) {
            return SCORE_PERFECT_MATCH; /* Rule 2: moving down, floor is below */
        }
        return SCORE_PASSED_MATCH;      /* Rule 3: same dir but passed */
    }

    /* Opposite direction — do not assign now */
    return SCORE_OPPOSITE_DIR;          /* Rule 4 */
}

/* ------------------------------------------------------------------ */
/*  Public API                                                        */
/* ------------------------------------------------------------------ */

void Dispatcher_Init(ElevatorState_t *elevA, ElevatorState_t *elevB) {
    s_elevA = elevA;
    s_elevB = elevB;
    s_hallUpCalls   = 0;
    s_hallDownCalls = 0;
    s_slaveUpAssign = 0;
    s_slaveDownAssign = 0;
}

void Dispatcher_RegisterHallCall(uint8 floor, uint8 dir) {
    ENTER_CRITICAL();
    if (dir == DIR_UP) {
        s_hallUpCalls |= (1U << floor);
    } else {
        s_hallDownCalls |= (1U << floor);
    }
    EXIT_CRITICAL();
}

void Dispatcher_Run(void) {
    /* Process each pending hall call */
    for (uint8 dir = DIR_UP; dir <= DIR_DOWN; dir++) {
        volatile uint8 *callMask = (dir == DIR_UP) ?
                                   &s_hallUpCalls : &s_hallDownCalls;

        for (uint8 floor = 0; floor <= FLOOR_MAX; floor++) {
            if (!(*callMask & (1U << floor))) continue;

            uint8 scoreA = ScoreElevator(s_elevA, floor, dir);
            uint8 scoreB = ScoreElevator(s_elevB, floor, dir);

            /* Skip if both are OPPOSITE_DIR or UNAVAILABLE */
            if (scoreA >= SCORE_OPPOSITE_DIR && scoreB >= SCORE_OPPOSITE_DIR) {
                continue; /* Defer this call to a later cycle */
            }

            ElevId_t winner = ELEV_NONE;

            if (scoreA < scoreB) {
                winner = ELEV_A;
            } else if (scoreB < scoreA) {
                winner = ELEV_B;
            } else {
                /* Tie-break: nearest elevator */
                uint8 distA = AbsDiff(s_elevA->currentFloor, floor);
                uint8 distB = AbsDiff(s_elevB->currentFloor, floor);
                winner = (distA <= distB) ? ELEV_A : ELEV_B;
            }

            /* Assign the call */
            ENTER_CRITICAL();
            *callMask &= ~(1U << floor); /* Remove from pending */
            EXIT_CRITICAL();

            if (winner == ELEV_A) {
                if (dir == DIR_UP) {
                    s_elevA->upRequests |= (1U << floor);
                } else {
                    s_elevA->downRequests |= (1U << floor);
                }
            } else {
                /* Elevator B — track for SPI command */
                if (dir == DIR_UP) {
                    s_slaveUpAssign |= (1U << floor);
                    s_elevB->upRequests |= (1U << floor);
                } else {
                    s_slaveDownAssign |= (1U << floor);
                    s_elevB->downRequests |= (1U << floor);
                }
            }
        }
    }
}

uint8 Dispatcher_GetSlaveAssignments(uint8 *pUpMask, uint8 *pDownMask) {
    *pUpMask   = s_slaveUpAssign;
    *pDownMask = s_slaveDownAssign;
    /* Clear after reading — will be re-populated next cycle if needed */
    s_slaveUpAssign   = 0;
    s_slaveDownAssign = 0;
    return (*pUpMask | *pDownMask) ? 1U : 0U;
}
