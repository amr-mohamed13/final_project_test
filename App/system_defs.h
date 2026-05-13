/**
 * system_defs.h
 *
 * Central header for the Collaborative Dual-Elevator System.
 * Shared types, SPI packet structure, FSM enums, pin mappings,
 * critical-section macros, and volatile flag declarations.
 */

#ifndef SYSTEM_DEFS_H
#define SYSTEM_DEFS_H

#include "Std_Types.h"

/* ================================================================== */
/*  CRITICAL SECTION MACROS                                           */
/* ================================================================== */
#define ENTER_CRITICAL()  __asm volatile ("cpsid i" ::: "memory")
#define EXIT_CRITICAL()   __asm volatile ("cpsie i" ::: "memory")

/* ================================================================== */
/*  SYSTEM CONSTANTS                                                  */
/* ================================================================== */
#define NUM_FLOORS              4U
#define FLOOR_MIN               0U      /* Ground / Floor 1 */
#define FLOOR_MAX               3U      /* Top    / Floor 4 */

/* Timing (milliseconds) */
#define SPI_EXCHANGE_PERIOD_MS  50U
#define TELEMETRY_PERIOD_MS     500U
#define DOOR_OPEN_TIME_MS       1000U
#define SPI_TIMEOUT_THRESHOLD   5U      /* consecutive misses */

/* Motor PWM duty-cycle percentages */
#define MOTOR_STOP              0U
#define MOTOR_SLOW              20U
#define MOTOR_FULL              100U

/* ================================================================== */
/*  FSM ENUMERATIONS                                                  */
/* ================================================================== */
typedef enum {
    ELEV_IDLE       = 0x00,
    ELEV_MOVING_UP  = 0x01,
    ELEV_MOVING_DOWN= 0x02,
    ELEV_DOORS_OPEN = 0x03,
    ELEV_EMERGENCY  = 0x04
} ElevState_t;

typedef enum {
    DIR_NONE = 0x00,
    DIR_UP   = 0x01,
    DIR_DOWN = 0x02
} ElevDir_t;

typedef enum {
    ELEV_A = 0x00,
    ELEV_B = 0x01,
    ELEV_NONE = 0xFF
} ElevId_t;

/* ================================================================== */
/*  SPI PACKET  (8-byte fixed frame)                                  */
/* ================================================================== */
#define SPI_FRAME_HEADER    0xA5U
#define SPI_FRAME_LEN       8U

typedef struct __attribute__((packed)) {
    uint8 header;           /* Must be 0xA5                        */
    uint8 elevId;           /* ELEV_A or ELEV_B                    */
    uint8 currentFloor;     /* 0 – 3                               */
    uint8 direction;        /* DIR_NONE / DIR_UP / DIR_DOWN        */
    uint8 state;            /* ElevState_t                         */
    uint8 requestMask;      /* Bitmask of pending floor requests   */
    uint8 cmdOrStatus;      /* Master→Slave: command / Slave→Master: extra status */
    uint8 checksum;         /* XOR of bytes [0..6]                 */
} SpiPacket_t;

/* ================================================================== */
/*  ELEVATOR STATE (volatile — shared between ISRs and main loop)     */
/* ================================================================== */
typedef struct {
    volatile uint8  currentFloor;       /* 0 – 3                     */
    volatile uint8  direction;          /* ElevDir_t                 */
    volatile uint8  fsmState;           /* ElevState_t               */
    volatile uint8  upRequests;         /* Bitmask: bit N = floor N  */
    volatile uint8  downRequests;       /* Bitmask                   */
    volatile uint8  cabinRequests;      /* Bitmask from cabin buttons*/
    volatile uint8  emergencyStop;      /* 1 = active                */
    volatile uint8  doorTimerActive;    /* 1 = door timer running    */
    volatile uint8  floorArrived;       /* Flag: new floor detected  */
    volatile uint8  arrivedFloor;       /* Which floor was detected  */
} ElevatorState_t;

/* ================================================================== */
/*  SPI COMMAND CODES  (Master → Slave via cmdOrStatus field)         */
/* ================================================================== */
#define SPI_CMD_NOP             0x00U
#define SPI_CMD_ASSIGN_FLOOR    0x01U   /* requestMask = assigned floors */
#define SPI_CMD_EMERGENCY_ALL   0x02U

/* ================================================================== */
/*  VOLATILE FLAGS  (set by ISRs, cleared by main loop)               */
/* ================================================================== */
extern volatile uint8 g_spiExchangeFlag;    /* TIM3: time to exchange      */
extern volatile uint8 g_telemetryFlag;      /* TIM4: time to send UART     */
extern volatile uint8 g_spiRxCompleteFlag;  /* Slave: SPI packet received  */
extern volatile uint8 g_spiTimeoutCount;    /* Master: consecutive fails   */
extern volatile uint8 g_slaveOnline;        /* Master: 1 = slave responding*/

/* ================================================================== */
/*  PIN MAPPING                                                       */
/* ================================================================== */

/* --- Common (both MCUs) --- */
#define PIN_SPI_SCK_PORT    GPIO_A
#define PIN_SPI_SCK_PIN     5U
#define PIN_SPI_MISO_PORT   GPIO_A
#define PIN_SPI_MISO_PIN    6U
#define PIN_SPI_MOSI_PORT   GPIO_A
#define PIN_SPI_MOSI_PIN    7U
#define PIN_SPI_CS_PORT     GPIO_A
#define PIN_SPI_CS_PIN      4U

#define PIN_MOTOR_PORT      GPIO_A
#define PIN_MOTOR_PIN       0U      /* TIM2_CH1, AF1 */

/* Cabin buttons (PB0-PB3) */
#define PIN_CABIN_PORT      GPIO_B
#define PIN_CABIN_F1        0U
#define PIN_CABIN_F2        1U
#define PIN_CABIN_F3        2U
#define PIN_CABIN_F4        3U

/* Emergency stop (PB4) */
#define PIN_EMERG_PORT      GPIO_B
#define PIN_EMERG_PIN       4U

/* Position sensors (PC12-PC15) */
#define PIN_POS_PORT        GPIO_C
#define PIN_POS_F1          12U
#define PIN_POS_F2          13U
#define PIN_POS_F3          14U
#define PIN_POS_F4          15U

/* --- Master only: Hallway call buttons (PB5-PB10) --- */
#ifdef MASTER_MCU
#define PIN_HALL_PORT       GPIO_B
#define PIN_HALL_U1         5U      /* Floor 1 UP   */
#define PIN_HALL_D2         6U      /* Floor 2 DOWN */
#define PIN_HALL_U2         7U      /* Floor 2 UP   */
#define PIN_HALL_D3         8U      /* Floor 3 DOWN */
#define PIN_HALL_U3         9U      /* Floor 3 UP   */
#define PIN_HALL_D4         10U     /* Floor 4 DOWN */

/* UART telemetry (PA2/PA3, USART2) */
#define PIN_UART_TX_PORT    GPIO_A
#define PIN_UART_TX_PIN     2U
#define PIN_UART_RX_PORT    GPIO_A
#define PIN_UART_RX_PIN     3U
#endif /* MASTER_MCU */

/* ================================================================== */
/*  TIMER ASSIGNMENTS                                                 */
/* ================================================================== */
#define MOTOR_TIMER         TIMER2
#define MOTOR_CHANNEL       CH1
#define MOTOR_PWM_PSC       159U    /* 16MHz / (159+1) = 100kHz       */
#define MOTOR_PWM_ARR       999U    /* 100kHz / (999+1) = 100Hz PWM   */

#ifdef MASTER_MCU
#define SPI_TIMER           TIMER3  /* 50ms periodic                  */
#define TELEMETRY_TIMER     TIMER4  /* 500ms periodic                 */
#endif

/* ================================================================== */
/*  HELPER FUNCTIONS                                                  */
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

#endif /* SYSTEM_DEFS_H */
