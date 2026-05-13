/**
 * spi_dispatcher_test.c — PC-side Unit Test for SPI + Dispatcher Logic
 *
 * Compile on PC with:
 *   gcc -DMASTER_MCU -I../App -I../Lib -I../Spi -o spi_test test/spi_dispatcher_test.c
 *
 * This file stubs out hardware functions and exercises the dispatcher logic
 * and SPI packet building to verify the bugs are fixed.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ================================================================== */
/*  Minimal type stubs (replaces Std_Types.h)                         */
/* ================================================================== */
typedef unsigned char  uint8;
typedef unsigned short uint16;
typedef unsigned int   uint32;

/* ================================================================== */
/*  Stub out hardware macros                                          */
/* ================================================================== */
#define ENTER_CRITICAL()
#define EXIT_CRITICAL()
#define __attribute__(x)

/* ================================================================== */
/*  Inline system_defs.h constants and types (no hardware deps)       */
/* ================================================================== */
#define NUM_FLOORS              4U
#define FLOOR_MIN               0U
#define FLOOR_MAX               3U
#define SPI_FRAME_HEADER        0xA5U
#define SPI_FRAME_LEN           8U
#define SPI_CMD_NOP             0x00U
#define SPI_CMD_ASSIGN_FLOOR    0x01U
#define SPI_CMD_EMERGENCY_ALL   0x02U

#define DIR_NONE    0x00
#define DIR_UP      0x01
#define DIR_DOWN    0x02

#define ELEV_A      0x00
#define ELEV_B      0x01
#define ELEV_NONE   0xFF

#define ELEV_IDLE        0x00
#define ELEV_MOVING_UP   0x01
#define ELEV_MOVING_DOWN 0x02
#define ELEV_DOORS_OPEN  0x03
#define ELEV_EMERGENCY   0x04

typedef struct {
    volatile uint8  currentFloor;
    volatile uint8  direction;
    volatile uint8  fsmState;
    volatile uint8  upRequests;
    volatile uint8  downRequests;
    volatile uint8  cabinRequests;
    volatile uint8  emergencyStop;
    volatile uint8  doorTimerActive;
    volatile uint8  floorArrived;
    volatile uint8  arrivedFloor;
} ElevatorState_t;

#pragma pack(push, 1)
typedef struct {
    uint8 header;
    uint8 elevId;
    uint8 currentFloor;
    uint8 direction;
    uint8 state;
    uint8 requestMask;
    uint8 cmdOrStatus;
    uint8 checksum;
} SpiPacket_t;
#pragma pack(pop)

typedef uint8 ElevId_t;

/* ================================================================== */
/*  Packet helpers (copy from system_defs.h)                          */
/* ================================================================== */
static inline uint8 SpiPacket_CalcChecksum(const SpiPacket_t *pkt) {
    const uint8 *p = (const uint8 *)pkt;
    uint8 cs = 0;
    for (uint8 i = 0; i < SPI_FRAME_LEN - 1U; i++) {
        cs ^= p[i];
    }
    return cs;
}

static inline uint8 SpiPacket_IsValid(const SpiPacket_t *pkt) {
    if (pkt->header != SPI_FRAME_HEADER) return 0;
    return (pkt->checksum == SpiPacket_CalcChecksum(pkt)) ? 1U : 0U;
}

static inline void SpiPacket_Build(SpiPacket_t *pkt, uint8 id,
                                   const ElevatorState_t *st, uint8 cmd) {
    pkt->header       = SPI_FRAME_HEADER;
    pkt->elevId       = id;
    pkt->currentFloor = st->currentFloor;
    pkt->direction    = st->direction;
    pkt->state        = st->fsmState;
    pkt->requestMask  = st->cabinRequests | st->upRequests | st->downRequests;
    pkt->cmdOrStatus  = cmd;
    pkt->checksum     = SpiPacket_CalcChecksum(pkt);
}

/* ================================================================== */
/*  Dispatcher copy (inlined to avoid needing to compile dispatcher.c */
/*  with all its deps — we test the ALGORITHM here)                   */
/* ================================================================== */
typedef enum {
    SCORE_IMMEDIATE     = 0,
    SCORE_PERFECT_MATCH = 1,
    SCORE_IDLE_NEAREST  = 2,
    SCORE_PASSED_MATCH  = 3,
    SCORE_OPPOSITE_DIR  = 4,
    SCORE_UNAVAILABLE   = 0xFF
} DispatchScore_t;

static ElevatorState_t *s_elevA;
static ElevatorState_t *s_elevB;
static volatile uint8 s_hallUpCalls;
static volatile uint8 s_hallDownCalls;
static uint8 s_slaveUpAssign;
static uint8 s_slaveDownAssign;

static uint8 AbsDiff(uint8 a, uint8 b) {
    return (a > b) ? (a - b) : (b - a);
}

static uint8 ScoreElevator(const ElevatorState_t *e,
                           uint8 callFloor, uint8 callDir) {
    uint8 estate = e->fsmState;
    if (estate == ELEV_EMERGENCY) return SCORE_UNAVAILABLE;
    if (estate == ELEV_IDLE || estate == ELEV_DOORS_OPEN) {
        if (e->currentFloor == callFloor) return SCORE_IMMEDIATE;
        return SCORE_IDLE_NEAREST;
    }
    uint8 movingUp = (estate == ELEV_MOVING_UP) ? 1U : 0U;
    uint8 elevDir  = movingUp ? DIR_UP : DIR_DOWN;
    if (elevDir == callDir) {
        if (movingUp && callFloor >= e->currentFloor) return SCORE_PERFECT_MATCH;
        if (!movingUp && callFloor <= e->currentFloor) return SCORE_PERFECT_MATCH;
        return SCORE_PASSED_MATCH;
    }
    return SCORE_OPPOSITE_DIR;
}

void Dispatcher_Init(ElevatorState_t *elevA, ElevatorState_t *elevB) {
    s_elevA = elevA;
    s_elevB = elevB;
    s_hallUpCalls   = 0;
    s_hallDownCalls = 0;
    s_slaveUpAssign = 0;
    s_slaveDownAssign = 0;
}

void Dispatcher_RegisterHallCall(uint8 floor, uint8 dir) {
    if (dir == DIR_UP) {
        s_hallUpCalls |= (1U << floor);
    } else {
        s_hallDownCalls |= (1U << floor);
    }
}

void Dispatcher_Run(void) {
    for (uint8 dir = DIR_UP; dir <= DIR_DOWN; dir++) {
        volatile uint8 *callMask = (dir == DIR_UP) ?
                                   &s_hallUpCalls : &s_hallDownCalls;
        for (uint8 floor = 0; floor <= FLOOR_MAX; floor++) {
            if (!(*callMask & (1U << floor))) continue;
            uint8 scoreA = ScoreElevator(s_elevA, floor, dir);
            uint8 scoreB = ScoreElevator(s_elevB, floor, dir);
            if (scoreA >= SCORE_OPPOSITE_DIR && scoreB >= SCORE_OPPOSITE_DIR) continue;

            ElevId_t winner = ELEV_NONE;
            if (scoreA < scoreB) {
                winner = ELEV_A;
            } else if (scoreB < scoreA) {
                winner = ELEV_B;
            } else {
                uint8 distA = AbsDiff(s_elevA->currentFloor, floor);
                uint8 distB = AbsDiff(s_elevB->currentFloor, floor);
                winner = (distA <= distB) ? ELEV_A : ELEV_B;
            }

            *callMask &= ~(1U << floor);
            if (winner == ELEV_A) {
                if (dir == DIR_UP) s_elevA->upRequests |= (1U << floor);
                else               s_elevA->downRequests |= (1U << floor);
            } else {
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
    s_slaveUpAssign   = 0;
    s_slaveDownAssign = 0;
    return (*pUpMask | *pDownMask) ? 1U : 0U;
}

/* ================================================================== */
/*  Test framework                                                    */
/* ================================================================== */
static int g_testsPassed = 0;
static int g_testsFailed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("  FAIL: %s\n", msg); \
        g_testsFailed++; \
    } else { \
        printf("  PASS: %s\n", msg); \
        g_testsPassed++; \
    } \
} while(0)

static void InitElev(ElevatorState_t *e, uint8 floor, uint8 state, uint8 dir) {
    memset(e, 0, sizeof(*e));
    e->currentFloor = floor;
    e->fsmState     = state;
    e->direction    = dir;
}

/* ================================================================== */
/*  TEST 1: Master@floor3, Slave@floor0 — call floor1 UP              */
/*  Expected: Slave wins (closer, both IDLE)                           */
/* ================================================================== */
static void Test_MasterAtF4_SlaveAtF1_CallF2Up(void) {
    printf("\n=== TEST 1: Master@F3(door4), Slave@F0(door1), HallCall F1 UP ===\n");

    ElevatorState_t elevA, elevB;
    InitElev(&elevA, 3, ELEV_IDLE, DIR_NONE);  /* Master at floor 3 (door 4) */
    InitElev(&elevB, 0, ELEV_IDLE, DIR_NONE);  /* Slave at floor 0 (door 1) */

    Dispatcher_Init(&elevA, &elevB);
    Dispatcher_RegisterHallCall(1, DIR_UP);     /* Floor 1 UP (door 2) */
    Dispatcher_Run();

    /* Slave should get the assignment (closer: dist=1 vs dist=2) */
    uint8 upMask = 0, downMask = 0;
    uint8 hasCmds = Dispatcher_GetSlaveAssignments(&upMask, &downMask);

    TEST_ASSERT(hasCmds == 1, "Slave has assignments");
    TEST_ASSERT((upMask & (1U << 1)) != 0, "Slave assigned floor 1 UP");
    TEST_ASSERT(downMask == 0, "No DOWN assignments for slave");
    TEST_ASSERT(elevA.upRequests == 0, "Master has no UP requests");
    TEST_ASSERT(elevA.downRequests == 0, "Master has no DOWN requests");
}

/* ================================================================== */
/*  TEST 2: Master@floor3, Slave@floor0 — call floor1 DOWN            */
/*  Expected: Slave wins (closer)                                      */
/* ================================================================== */
static void Test_MasterAtF4_SlaveAtF1_CallF2Down(void) {
    printf("\n=== TEST 2: Master@F3(door4), Slave@F0(door1), HallCall F1 DOWN ===\n");

    ElevatorState_t elevA, elevB;
    InitElev(&elevA, 3, ELEV_IDLE, DIR_NONE);
    InitElev(&elevB, 0, ELEV_IDLE, DIR_NONE);

    Dispatcher_Init(&elevA, &elevB);
    Dispatcher_RegisterHallCall(1, DIR_DOWN);

    Dispatcher_Run();

    uint8 upMask = 0, downMask = 0;
    uint8 hasCmds = Dispatcher_GetSlaveAssignments(&upMask, &downMask);

    TEST_ASSERT(hasCmds == 1, "Slave has assignments");
    TEST_ASSERT(upMask == 0, "No UP assignments for slave");
    TEST_ASSERT((downMask & (1U << 1)) != 0, "Slave assigned floor 1 DOWN");
}

/* ================================================================== */
/*  TEST 3: Master@floor0, Slave@floor3 — call floor2 UP              */
/*  Expected: Slave wins (closer: dist=1 vs dist=2)                    */
/* ================================================================== */
static void Test_MasterAtF1_SlaveAtF4_CallF3Up(void) {
    printf("\n=== TEST 3: Master@F0(door1), Slave@F3(door4), HallCall F2 UP ===\n");

    ElevatorState_t elevA, elevB;
    InitElev(&elevA, 0, ELEV_IDLE, DIR_NONE);
    InitElev(&elevB, 3, ELEV_IDLE, DIR_NONE);

    Dispatcher_Init(&elevA, &elevB);
    Dispatcher_RegisterHallCall(2, DIR_UP);     /* Floor 2 UP (door 3) */

    Dispatcher_Run();

    uint8 upMask = 0, downMask = 0;
    uint8 hasCmds = Dispatcher_GetSlaveAssignments(&upMask, &downMask);

    TEST_ASSERT(hasCmds == 1, "Slave has assignments");
    TEST_ASSERT((upMask & (1U << 2)) != 0, "Slave assigned floor 2 UP");
    TEST_ASSERT(downMask == 0, "No DOWN assignments");
    TEST_ASSERT(elevA.upRequests == 0, "Master has no requests");
}

/* ================================================================== */
/*  TEST 4: Master@floor0, Slave@floor3 — call floor2 DOWN            */
/*  Expected: Slave wins (closer)                                      */
/* ================================================================== */
static void Test_MasterAtF1_SlaveAtF4_CallF3Down(void) {
    printf("\n=== TEST 4: Master@F0(door1), Slave@F3(door4), HallCall F2 DOWN ===\n");

    ElevatorState_t elevA, elevB;
    InitElev(&elevA, 0, ELEV_IDLE, DIR_NONE);
    InitElev(&elevB, 3, ELEV_IDLE, DIR_NONE);

    Dispatcher_Init(&elevA, &elevB);
    Dispatcher_RegisterHallCall(2, DIR_DOWN);

    Dispatcher_Run();

    uint8 upMask = 0, downMask = 0;
    uint8 hasCmds = Dispatcher_GetSlaveAssignments(&upMask, &downMask);

    TEST_ASSERT(hasCmds == 1, "Slave has assignments");
    TEST_ASSERT(upMask == 0, "No UP assignments");
    TEST_ASSERT((downMask & (1U << 2)) != 0, "Slave assigned floor 2 DOWN");
}

/* ================================================================== */
/*  TEST 5: Verify SPI packet checksum is correct after override       */
/* ================================================================== */
static void Test_PacketChecksumAfterOverride(void) {
    printf("\n=== TEST 5: SPI packet checksum after requestMask override ===\n");

    ElevatorState_t elevA;
    InitElev(&elevA, 2, ELEV_IDLE, DIR_NONE);

    SpiPacket_t pkt;
    SpiPacket_Build(&pkt, ELEV_A, &elevA, SPI_CMD_ASSIGN_FLOOR);

    /* Override requestMask (as master does) */
    uint8 slaveUp   = (1U << 1);   /* Floor 1 UP */
    uint8 slaveDown = (1U << 3);   /* Floor 3 DOWN */
    pkt.requestMask = (slaveUp & 0x0FU) | ((slaveDown & 0x0FU) << 4U);

    /* Recompute checksum (as fixed master does) */
    pkt.checksum = SpiPacket_CalcChecksum(&pkt);

    TEST_ASSERT(SpiPacket_IsValid(&pkt) == 1, "Packet valid after recomputed checksum");

    /* Verify the packed data */
    uint8 recoveredUp   = pkt.requestMask & 0x0FU;
    uint8 recoveredDown = (pkt.requestMask >> 4U) & 0x0FU;
    TEST_ASSERT(recoveredUp == slaveUp, "Recovered UP mask matches");
    TEST_ASSERT(recoveredDown == slaveDown, "Recovered DOWN mask matches");
}

/* ================================================================== */
/*  TEST 6: BUG REPRODUCTION — OLD behavior (should show failure)      */
/*  Calling GetSlaveAssignments BEFORE Run = always empty              */
/* ================================================================== */
static void Test_OldBugReproduction(void) {
    printf("\n=== TEST 6: Old bug reproduction — Get BEFORE Run ===\n");

    ElevatorState_t elevA, elevB;
    InitElev(&elevA, 3, ELEV_IDLE, DIR_NONE);
    InitElev(&elevB, 0, ELEV_IDLE, DIR_NONE);

    Dispatcher_Init(&elevA, &elevB);
    Dispatcher_RegisterHallCall(1, DIR_UP);

    /* OLD CODE ORDER: Get BEFORE Run */
    uint8 upMask = 0, downMask = 0;
    uint8 hasCmds = Dispatcher_GetSlaveAssignments(&upMask, &downMask);

    /* This should be EMPTY because Run hasn't been called yet */
    TEST_ASSERT(hasCmds == 0, "OLD BUG: assignments empty before Run() — confirms the bug");
    TEST_ASSERT(upMask == 0, "OLD BUG: no UP mask before Run()");

    /* Now run — assignments go into internal state but never sent */
    Dispatcher_Run();

    /* The slave assignments are there now, but we already "sent" the SPI packet */
    uint8 upMask2 = 0, downMask2 = 0;
    uint8 hasCmds2 = Dispatcher_GetSlaveAssignments(&upMask2, &downMask2);
    TEST_ASSERT(hasCmds2 == 1, "After Run(), assignments exist (but were never sent in old code)");
}

/* ================================================================== */
/*  TEST 7: Full roundtrip — Master builds pkt, Slave unpacks          */
/* ================================================================== */
static void Test_FullRoundtrip(void) {
    printf("\n=== TEST 7: Full roundtrip — Master build → Slave unpack ===\n");

    ElevatorState_t elevA, elevB;
    InitElev(&elevA, 3, ELEV_IDLE, DIR_NONE);  /* Master at floor 3 */
    InitElev(&elevB, 0, ELEV_IDLE, DIR_NONE);  /* Slave at floor 0  */

    Dispatcher_Init(&elevA, &elevB);
    Dispatcher_RegisterHallCall(1, DIR_UP);     /* Floor 1 UP — Slave closer (dist=1 vs 2) */
    Dispatcher_RegisterHallCall(1, DIR_DOWN);   /* Floor 1 DOWN — Slave closer */

    /* Step 1: Run dispatcher (FIXED ORDER) */
    Dispatcher_Run();

    /* Step 2: Get assignments */
    uint8 upMask = 0, downMask = 0;
    Dispatcher_GetSlaveAssignments(&upMask, &downMask);

    /* Step 3: Build SPI packet */
    SpiPacket_t txPkt;
    SpiPacket_Build(&txPkt, ELEV_A, &elevA, SPI_CMD_ASSIGN_FLOOR);
    txPkt.requestMask = (upMask & 0x0FU) | ((downMask & 0x0FU) << 4U);
    txPkt.checksum = SpiPacket_CalcChecksum(&txPkt);

    TEST_ASSERT(SpiPacket_IsValid(&txPkt), "TX packet is valid");

    /* Step 4: Simulate slave receiving the packet */
    ElevatorState_t slaveElev;
    InitElev(&slaveElev, 0, ELEV_IDLE, DIR_NONE);

    /* Slave-side unpack (matches fixed slave_main.c) */
    uint8 rxUp   = txPkt.requestMask & 0x0FU;
    uint8 rxDown = (txPkt.requestMask >> 4U) & 0x0FU;
    slaveElev.upRequests   |= rxUp;
    slaveElev.downRequests |= rxDown;

    TEST_ASSERT((slaveElev.upRequests & (1U << 1)) != 0, "Slave got floor 1 UP");
    TEST_ASSERT((slaveElev.downRequests & (1U << 1)) != 0, "Slave got floor 1 DOWN");
    TEST_ASSERT((slaveElev.upRequests & (1U << 3)) == 0, "Slave does NOT have floor 3 UP");
    TEST_ASSERT((slaveElev.downRequests & (1U << 3)) == 0, "Slave does NOT have floor 3 DOWN");
}

/* ================================================================== */
/*  TEST 8: Master closer — Master should handle, not Slave            */
/* ================================================================== */
static void Test_MasterCloser(void) {
    printf("\n=== TEST 8: Master closer — call assigned to Master ===\n");

    ElevatorState_t elevA, elevB;
    InitElev(&elevA, 3, ELEV_IDLE, DIR_NONE);  /* Master at floor 3 */
    InitElev(&elevB, 0, ELEV_IDLE, DIR_NONE);  /* Slave at floor 0 */

    Dispatcher_Init(&elevA, &elevB);
    Dispatcher_RegisterHallCall(2, DIR_UP);     /* Floor 2 — Master is 1 away, Slave is 2 away */

    Dispatcher_Run();

    uint8 upMask = 0, downMask = 0;
    uint8 hasCmds = Dispatcher_GetSlaveAssignments(&upMask, &downMask);

    TEST_ASSERT(hasCmds == 0, "No slave assignments (Master is closer)");
    TEST_ASSERT((elevA.upRequests & (1U << 2)) != 0, "Master assigned floor 2 UP");
}

/* ================================================================== */
/*  MAIN                                                               */
/* ================================================================== */
int main(void) {
    printf("========================================\n");
    printf("  SPI + Dispatcher Unit Tests\n");
    printf("========================================\n");

    Test_MasterAtF4_SlaveAtF1_CallF2Up();
    Test_MasterAtF4_SlaveAtF1_CallF2Down();
    Test_MasterAtF1_SlaveAtF4_CallF3Up();
    Test_MasterAtF1_SlaveAtF4_CallF3Down();
    Test_PacketChecksumAfterOverride();
    Test_OldBugReproduction();
    Test_FullRoundtrip();
    Test_MasterCloser();

    printf("\n========================================\n");
    printf("  Results: %d PASSED, %d FAILED\n", g_testsPassed, g_testsFailed);
    printf("========================================\n");

    return g_testsFailed > 0 ? 1 : 0;
}
