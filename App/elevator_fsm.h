/**
 * elevator_fsm.h — Elevator Finite State Machine
 */

#ifndef ELEVATOR_FSM_H
#define ELEVATOR_FSM_H

#include "system_defs.h"

/** Initialise an elevator to IDLE at floor 0. */
void ElevFsm_Init(ElevatorState_t *elev);

/**
 * @brief  Run one FSM update tick.
 *         Called from the main loop on each iteration.
 *         Reads volatile flags, drives motor PWM, transitions states.
 */
void ElevFsm_Update(ElevatorState_t *elev);

/** Add a cabin button request. */
void ElevFsm_AddCabinRequest(ElevatorState_t *elev, uint8 floor);

/** Force emergency stop. */
void ElevFsm_EmergencyStop(ElevatorState_t *elev);

/** Release emergency stop. */
void ElevFsm_EmergencyRelease(ElevatorState_t *elev);

#endif /* ELEVATOR_FSM_H */
