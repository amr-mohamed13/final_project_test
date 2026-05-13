/**
 * elevator_fsm.c — Elevator Finite State Machine Implementation
 *
 * States: IDLE → MOVING_UP/DOWN → DOORS_OPEN → IDLE
 *         Any state → EMERGENCY (via emergency stop)
 */

#include "elevator_fsm.h"
#include "Pwm.h"
#include "Timer.h"
#include "system_defs.h"    /* ENTER_CRITICAL / EXIT_CRITICAL */

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                  */
/* ------------------------------------------------------------------ */

/** Check if any request exists above/at/below the current floor. */
static uint8 HasRequestAbove(const ElevatorState_t *e) {
    uint8 mask = e->cabinRequests | e->upRequests | e->downRequests;
    /* Any bit set for floors above current? */
    uint8 aboveMask = (uint8)(0xFFU << (e->currentFloor + 1U));
    return (mask & aboveMask) ? 1U : 0U;
}

static uint8 HasRequestBelow(const ElevatorState_t *e) {
    uint8 mask = e->cabinRequests | e->upRequests | e->downRequests;
    uint8 belowMask = (uint8)((1U << e->currentFloor) - 1U);
    return (mask & belowMask) ? 1U : 0U;
}

static uint8 HasRequestAtFloor(const ElevatorState_t *e, uint8 floor) {
    uint8 mask = e->cabinRequests | e->upRequests | e->downRequests;
    return (mask & (1U << floor)) ? 1U : 0U;
}

static uint8 HasAnyRequest(const ElevatorState_t *e) {
    return (e->cabinRequests | e->upRequests | e->downRequests) ? 1U : 0U;
}

/** Clear all requests for the given floor. */
static void ClearFloorRequests(ElevatorState_t *e, uint8 floor) {
    uint8 bit = (uint8)(1U << floor);
    /* Protect read-modify-write of volatile bitmasks shared with EXTI ISRs */
    ENTER_CRITICAL();
    e->cabinRequests &= ~bit;
    e->upRequests    &= ~bit;
    e->downRequests  &= ~bit;
    EXIT_CRITICAL();
}

/** Set motor PWM duty cycle. */
static void SetMotor(uint8 dutyPercent) {
    Pwm_SetDutyPercent(MOTOR_TIMER, MOTOR_CHANNEL, dutyPercent);
}

/** Determine the next target floor based on current direction and requests. */
static uint8 FindNextTarget(const ElevatorState_t *e) {
    uint8 allReq = e->cabinRequests | e->upRequests | e->downRequests;
    if (allReq == 0) return 0xFF; /* No target */

    if (e->direction == DIR_UP) {
        /* Find nearest request above */
        for (uint8 f = e->currentFloor; f <= FLOOR_MAX; f++) {
            if (allReq & (1U << f)) return f;
        }
        /* Wrap: find nearest below (will reverse) */
        for (uint8 f = FLOOR_MAX; f > 0; f--) {
            if (allReq & (1U << (f - 1))) return (f - 1);
        }
    } else if (e->direction == DIR_DOWN) {
        /* Find nearest request below */
        for (uint8 f = e->currentFloor; f > 0; f--) {
            if (allReq & (1U << (f - 1))) return (f - 1);
        }
        if (allReq & (1U << e->currentFloor)) return e->currentFloor;
        /* Wrap: find nearest above (will reverse) */
        for (uint8 f = 0; f <= FLOOR_MAX; f++) {
            if (allReq & (1U << f)) return f;
        }
    } else {
        /* DIR_NONE: find nearest */
        uint8 bestFloor = 0;
        uint8 bestDist  = 0xFF;
        for (uint8 f = 0; f <= FLOOR_MAX; f++) {
            if (allReq & (1U << f)) {
                uint8 dist = (f > e->currentFloor) ?
                             (f - e->currentFloor) :
                             (e->currentFloor - f);
                if (dist < bestDist) {
                    bestDist  = dist;
                    bestFloor = f;
                }
            }
        }
        return bestFloor;
    }
    return 0xFF;
}

/* Door-open completion callback (set by Timer_DelayMsAsync) */
static ElevatorState_t *s_doorElev = (void *)0;

static void DoorTimerCallback(void) {
    if (s_doorElev != (void *)0) {
        s_doorElev->doorTimerActive = 0;
    }
}

/* ================================================================== */
/*  PUBLIC API                                                        */
/* ================================================================== */

void ElevFsm_Init(ElevatorState_t *elev) {
    elev->currentFloor   = 0;
    elev->direction      = DIR_NONE;
    elev->fsmState       = ELEV_IDLE;
    elev->upRequests     = 0;
    elev->downRequests   = 0;
    elev->cabinRequests  = 0;
    elev->emergencyStop  = 0;
    elev->doorTimerActive= 0;
    elev->floorArrived   = 0;
    elev->arrivedFloor   = 0;
    SetMotor(MOTOR_STOP);
}

void ElevFsm_Update(ElevatorState_t *elev) {
    /* --- Check emergency first (highest priority) --- */
    if (elev->emergencyStop) {
        if (elev->fsmState != ELEV_EMERGENCY) {
            SetMotor(MOTOR_STOP);
            elev->fsmState  = ELEV_EMERGENCY;
            elev->direction = DIR_NONE;
        }
        return;
    }

    /* --- If returning from emergency --- */
    if (elev->fsmState == ELEV_EMERGENCY) {
        elev->fsmState = ELEV_IDLE;
        elev->direction = DIR_NONE;
        /* Fall through to IDLE handler */
    }

    /* --- Process floor-arrival event --- */
    if (elev->floorArrived) {
        elev->floorArrived  = 0;
        elev->currentFloor  = elev->arrivedFloor;

        /* Check if we should stop at this floor */
        if (HasRequestAtFloor(elev, elev->currentFloor)) {
            SetMotor(MOTOR_STOP);
            ClearFloorRequests(elev, elev->currentFloor);
            elev->fsmState = ELEV_DOORS_OPEN;

            /* Start door timer using TIM5 (one-shot async) */
            s_doorElev = elev;
            elev->doorTimerActive = 1;
            Timer_DelayMsAsync(TIMER5, DOOR_OPEN_TIME_MS, DoorTimerCallback);
            return;
        }
    }

    /* --- State machine --- */
    switch ((ElevState_t)elev->fsmState) {

    case ELEV_IDLE: {
        if (!HasAnyRequest(elev)) return;

        uint8 target = FindNextTarget(elev);
        if (target == 0xFF) return;

        if (target == elev->currentFloor) {
            /* Already here — open doors */
            ClearFloorRequests(elev, elev->currentFloor);
            elev->fsmState = ELEV_DOORS_OPEN;
            s_doorElev = elev;
            elev->doorTimerActive = 1;
            Timer_DelayMsAsync(TIMER5, DOOR_OPEN_TIME_MS, DoorTimerCallback);
        } else if (target > elev->currentFloor) {
            elev->direction = DIR_UP;
            elev->fsmState  = ELEV_MOVING_UP;
            SetMotor(MOTOR_FULL);
        } else {
            elev->direction = DIR_DOWN;
            elev->fsmState  = ELEV_MOVING_DOWN;
            SetMotor(MOTOR_FULL);
        }
        break;
    }

    case ELEV_MOVING_UP: {
        if (!HasRequestAbove(elev) && !HasRequestAtFloor(elev, elev->currentFloor)) {
            /* No more requests in this direction */
            if (HasRequestBelow(elev)) {
                elev->direction = DIR_DOWN;
                elev->fsmState  = ELEV_MOVING_DOWN;
            } else {
                SetMotor(MOTOR_STOP);
                elev->direction = DIR_NONE;
                elev->fsmState  = ELEV_IDLE;
            }
        }
        /* Motor remains at FULL — floor arrival is detected via position sensor ISR */
        break;
    }

    case ELEV_MOVING_DOWN: {
        if (!HasRequestBelow(elev) && !HasRequestAtFloor(elev, elev->currentFloor)) {
            if (HasRequestAbove(elev)) {
                elev->direction = DIR_UP;
                elev->fsmState  = ELEV_MOVING_UP;
            } else {
                SetMotor(MOTOR_STOP);
                elev->direction = DIR_NONE;
                elev->fsmState  = ELEV_IDLE;
            }
        }
        break;
    }

    case ELEV_DOORS_OPEN: {
        /* Wait for door timer to expire */
        if (!elev->doorTimerActive) {
            /* Doors closed — decide next action */
            if (HasAnyRequest(elev)) {
                uint8 target = FindNextTarget(elev);
                if (target != 0xFF && target > elev->currentFloor) {
                    elev->direction = DIR_UP;
                    elev->fsmState  = ELEV_MOVING_UP;
                    SetMotor(MOTOR_FULL);
                } else if (target != 0xFF && target < elev->currentFloor) {
                    elev->direction = DIR_DOWN;
                    elev->fsmState  = ELEV_MOVING_DOWN;
                    SetMotor(MOTOR_FULL);
                } else {
                    elev->direction = DIR_NONE;
                    elev->fsmState  = ELEV_IDLE;
                }
            } else {
                elev->direction = DIR_NONE;
                elev->fsmState  = ELEV_IDLE;
            }
        }
        break;
    }

    case ELEV_EMERGENCY:
        /* Handled at top of function */
        break;
    }
}

void ElevFsm_AddCabinRequest(ElevatorState_t *elev, uint8 floor) {
    if (floor <= FLOOR_MAX) {
        elev->cabinRequests |= (1U << floor);
    }
}

void ElevFsm_EmergencyStop(ElevatorState_t *elev) {
    elev->emergencyStop = 1;
}

void ElevFsm_EmergencyRelease(ElevatorState_t *elev) {
    elev->emergencyStop = 0;
}
