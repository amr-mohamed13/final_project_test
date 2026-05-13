/**
 * dispatcher.h — Task Allocation Algorithm (Master Only)
 */

#ifndef DISPATCHER_H
#define DISPATCHER_H

#include "system_defs.h"

/**
 * @brief  Initialise the dispatcher with pointers to both elevator states.
 */
void Dispatcher_Init(ElevatorState_t *elevA, ElevatorState_t *elevB);

/**
 * @brief  Register a new hallway call.
 * @param  floor  0–3
 * @param  dir    DIR_UP or DIR_DOWN
 */
void Dispatcher_RegisterHallCall(uint8 floor, uint8 dir);

/**
 * @brief  Run the dispatcher algorithm.
 *         Evaluates all pending hall calls and assigns them to the
 *         optimal elevator using the 5-priority rule set.
 *         Should be called every SPI exchange cycle (50ms).
 */
void Dispatcher_Run(void);

/**
 * @brief  Get the pending request masks assigned to Elevator B (Slave).
 *         The Master packs these into the SPI packet.
 * @param  pUpMask   Output: bitmask of floors assigned UP direction
 * @param  pDownMask Output: bitmask of floors assigned DOWN direction
 * @return 1 if any assignments pending, 0 if none.
 */
uint8 Dispatcher_GetSlaveAssignments(uint8 *pUpMask, uint8 *pDownMask);

#endif /* DISPATCHER_H */
