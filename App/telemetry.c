/**
 * telemetry.c — UART DMA Telemetry (Master Only)
 *
 * Formats a human-readable status string and sends it over
 * USART2 via DMA for zero CPU overhead.
 */

#include "telemetry.h"
#include "Uart.h"

/* Static TX buffer — must persist until DMA completes */
static uint8 s_txBuf[80];

static const char *StateStr(uint8 st) {
    switch ((ElevState_t)st) {
        case ELEV_IDLE:        return "IDLE";
        case ELEV_MOVING_UP:   return "M_UP";
        case ELEV_MOVING_DOWN: return "M_DN";
        case ELEV_DOORS_OPEN:  return "DOOR";
        case ELEV_EMERGENCY:   return "EMRG";
        default:               return "????";
    }
}

static const char *DirStr(uint8 d) {
    switch ((ElevDir_t)d) {
        case DIR_UP:   return "UP";
        case DIR_DOWN: return "DN";
        default:       return "--";
    }
}

/* Simple uint8-safe string copy, returns chars written */
static uint16 StrCopy(uint8 *dst, const char *src) {
    uint16 n = 0;
    while (*src) { dst[n++] = (uint8)*src++; }
    return n;
}

void Telemetry_Send(const ElevatorState_t *elevA, const ElevatorState_t *elevB) {
    if (!Uart_IsTxReady()) return;

    uint16 pos = 0;

    /* Elevator A */
    pos += StrCopy(&s_txBuf[pos], "A:F");
    s_txBuf[pos++] = '1' + elevA->currentFloor;
    s_txBuf[pos++] = ',';
    pos += StrCopy(&s_txBuf[pos], DirStr(elevA->direction));
    s_txBuf[pos++] = ',';
    pos += StrCopy(&s_txBuf[pos], StateStr(elevA->fsmState));

    s_txBuf[pos++] = '|';

    /* Elevator B */
    pos += StrCopy(&s_txBuf[pos], "B:F");
    s_txBuf[pos++] = '1' + elevB->currentFloor;
    s_txBuf[pos++] = ',';
    pos += StrCopy(&s_txBuf[pos], DirStr(elevB->direction));
    s_txBuf[pos++] = ',';
    pos += StrCopy(&s_txBuf[pos], StateStr(elevB->fsmState));

    s_txBuf[pos++] = '\r';
    s_txBuf[pos++] = '\n';

    Uart_Send(s_txBuf, pos);
}
