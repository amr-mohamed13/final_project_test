/**
 * telemetry.h — UART DMA Telemetry (Master Only)
 */

#ifndef TELEMETRY_H
#define TELEMETRY_H

#include "system_defs.h"

/**
 * @brief  Format and send elevator status over UART via DMA.
 *         Output format: "A:F<n>,<DIR>,<STATE>|B:F<n>,<DIR>,<STATE>\r\n"
 */
void Telemetry_Send(const ElevatorState_t *elevA, const ElevatorState_t *elevB);

#endif /* TELEMETRY_H */
