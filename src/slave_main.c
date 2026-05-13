/**
 * slave_main.c — Slave MCU Entry Point
 *
 * Elevator B + SPI Slave
 * STM32F401xE @ 16 MHz HSI
 */

#include "system_defs.h"
#include "Rcc.h"
#include "Gpio.h"
#include "Exti.h"
#include "Timer.h"
#include "Pwm.h"
#include "Spi.h"
#include "elevator_fsm.h"

/* ================================================================== */
/*  GLOBAL VOLATILE FLAGS                                             */
/* ================================================================== */
volatile uint8 g_spiExchangeFlag   = 0;  /* unused on Slave */
volatile uint8 g_telemetryFlag     = 0;  /* unused on Slave */
volatile uint8 g_spiRxCompleteFlag = 0;
volatile uint8 g_spiTimeoutCount   = 0;
volatile uint8 g_slaveOnline       = 1;

/* ================================================================== */
/*  ELEVATOR STATE                                                    */
/* ================================================================== */
ElevatorState_t g_elevB;

/* SPI timeout: if no packet in ~200ms (4 × 50ms cycles) */
static volatile uint8 s_spiWatchdog = 0;

static void SpiWatchdogCallback(void) {
    s_spiWatchdog++;
    if (s_spiWatchdog >= SPI_TIMEOUT_THRESHOLD) {
        g_slaveOnline = 0; /* Enter independent mode */
    }
}

/* ================================================================== */
/*  GPIO INITIALISATION                                               */
/* ================================================================== */
static void InitGpio(void) {
    /* SPI1 Slave pins: AF5 */
    Gpio_Init(PIN_SPI_SCK_PORT,  PIN_SPI_SCK_PIN,  GPIO_AF, GPIO_PUSH_PULL);
    Gpio_SetAF(PIN_SPI_SCK_PORT, PIN_SPI_SCK_PIN,  GPIO_AF5);
    Gpio_Init(PIN_SPI_MISO_PORT, PIN_SPI_MISO_PIN, GPIO_AF, GPIO_PUSH_PULL);
    Gpio_SetAF(PIN_SPI_MISO_PORT,PIN_SPI_MISO_PIN, GPIO_AF5);
    Gpio_Init(PIN_SPI_MOSI_PORT, PIN_SPI_MOSI_PIN, GPIO_AF, GPIO_PUSH_PULL);
    Gpio_SetAF(PIN_SPI_MOSI_PORT,PIN_SPI_MOSI_PIN, GPIO_AF5);

    /* NSS = AF5 (hardware managed) */
    Gpio_Init(PIN_SPI_CS_PORT, PIN_SPI_CS_PIN, GPIO_AF, GPIO_PUSH_PULL);
    Gpio_SetAF(PIN_SPI_CS_PORT, PIN_SPI_CS_PIN, GPIO_AF5);

    /* Motor PWM: AF1 (TIM2_CH1) */
    Gpio_Init(PIN_MOTOR_PORT, PIN_MOTOR_PIN, GPIO_AF, GPIO_PUSH_PULL);
    Gpio_SetAF(PIN_MOTOR_PORT, PIN_MOTOR_PIN, GPIO_AF1);

    /* Cabin buttons: PB0-PB3, input pull-up */
    Gpio_Init(PIN_CABIN_PORT, PIN_CABIN_F1, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(PIN_CABIN_PORT, PIN_CABIN_F2, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(PIN_CABIN_PORT, PIN_CABIN_F3, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(PIN_CABIN_PORT, PIN_CABIN_F4, GPIO_INPUT, GPIO_PULL_UP);

    /* Emergency: PB4, input pull-up */
    Gpio_Init(PIN_EMERG_PORT, PIN_EMERG_PIN, GPIO_INPUT, GPIO_PULL_UP);

    /* Position sensors: PC12-PC15, input pull-down */
    Gpio_Init(PIN_POS_PORT, PIN_POS_F1, GPIO_INPUT, GPIO_PULL_DOWN);
    Gpio_Init(PIN_POS_PORT, PIN_POS_F2, GPIO_INPUT, GPIO_PULL_DOWN);
    Gpio_Init(PIN_POS_PORT, PIN_POS_F3, GPIO_INPUT, GPIO_PULL_DOWN);
    Gpio_Init(PIN_POS_PORT, PIN_POS_F4, GPIO_INPUT, GPIO_PULL_DOWN);
}

/* ================================================================== */
/*  EXTI INITIALISATION                                               */
/* ================================================================== */
static void InitExti(void) {
    /* Cabin buttons: PB0-PB3, falling edge, priority 4 */
    Exti_Init(EXTI_PORT_B, PIN_CABIN_F1, EXTI_FALLING, 4U);
    Exti_Init(EXTI_PORT_B, PIN_CABIN_F2, EXTI_FALLING, 4U);
    Exti_Init(EXTI_PORT_B, PIN_CABIN_F3, EXTI_FALLING, 4U);
    Exti_Init(EXTI_PORT_B, PIN_CABIN_F4, EXTI_FALLING, 4U);

    /* Emergency: PB4, both edges, priority 0 (HIGHEST) */
    Exti_Init(EXTI_PORT_B, PIN_EMERG_PIN, EXTI_BOTH, 0U);

    /* Position sensors: PC12-PC15, rising edge, priority 2 */
    Exti_Init(EXTI_PORT_C, PIN_POS_F1, EXTI_RISING, 2U);
    Exti_Init(EXTI_PORT_C, PIN_POS_F2, EXTI_RISING, 2U);
    Exti_Init(EXTI_PORT_C, PIN_POS_F3, EXTI_RISING, 2U);
    Exti_Init(EXTI_PORT_C, PIN_POS_F4, EXTI_RISING, 2U);
}

/* ================================================================== */
/*  SPI COMMAND PROCESSING                                            */
/* ================================================================== */
static SpiPacket_t s_rxPkt;
static SpiPacket_t s_txPkt;

static void PreloadSpiStatus(void) {
    /* Protect g_elevB from concurrent EXTI modifications during read */
    ENTER_CRITICAL();
    SpiPacket_Build(&s_txPkt, ELEV_B, &g_elevB, SPI_CMD_NOP);
    EXIT_CRITICAL();
    Spi_SlavePreload((const uint8 *)&s_txPkt, SPI_FRAME_LEN);
}

static void ProcessMasterCommand(void) {
    uint8 n = Spi_SlaveRead((uint8 *)&s_rxPkt, SPI_FRAME_LEN);
    if (n == 0) return;
    if (!SpiPacket_IsValid(&s_rxPkt)) return;

    /* Reset watchdog */
    s_spiWatchdog = 0;
    g_slaveOnline = 1;

    /* Process command */
    switch (s_rxPkt.cmdOrStatus) {
    case SPI_CMD_ASSIGN_FLOOR: {
        /* Master packs: UP in low nibble, DOWN in high nibble */
        uint8 upMask   = s_rxPkt.requestMask & 0x0FU;
        uint8 downMask = (s_rxPkt.requestMask >> 4U) & 0x0FU;
        /* Protect read-modify-write of shared request bitmasks */
        ENTER_CRITICAL();
        g_elevB.upRequests   |= upMask;
        g_elevB.downRequests |= downMask;
        EXIT_CRITICAL();
        break;
    }

    case SPI_CMD_EMERGENCY_ALL:
        ElevFsm_EmergencyStop(&g_elevB);
        break;

    case SPI_CMD_NOP:
    default:
        break;
    }

    /* Re-preload our status for next exchange */
    PreloadSpiStatus();
}

/* ================================================================== */
/*  MAIN                                                              */
/* ================================================================== */
int main(void) {
    /* 1. Clock setup */
    Rcc_Init();
    Rcc_Enable(RCC_GPIOA);
    Rcc_Enable(RCC_GPIOB);
    Rcc_Enable(RCC_GPIOC);
    Rcc_Enable(RCC_SYSCFG);
    Rcc_Enable(RCC_TIM2);
    Rcc_Enable(RCC_TIM3);
    Rcc_Enable(RCC_TIM5);
    Rcc_Enable(RCC_SPI1);

    /* 2. GPIO */
    InitGpio();

    /* 3. EXTI */
    InitExti();

    /* 4. SPI Slave */
    Spi_InitSlave();

    /* 5. Motor PWM (TIM2 CH1) */
    Pwm_Init(MOTOR_TIMER, MOTOR_CHANNEL, MOTOR_PWM_PSC, MOTOR_PWM_ARR);
    Pwm_Start(MOTOR_TIMER, MOTOR_CHANNEL);

    /* 6. FSM init */
    ElevFsm_Init(&g_elevB);

    /* 7. Pre-load initial SPI status */
    PreloadSpiStatus();

    /* 8. SPI watchdog: TIM3 every 50ms */
    Timer_StartPeriodic(TIMER3, 15999U, 49U, SpiWatchdogCallback);

    /* ============ SUPERLOOP ============ */
    while (1) {
        /* Process SPI command from Master */
        if (g_spiRxCompleteFlag) {
            g_spiRxCompleteFlag = 0;
            ProcessMasterCommand();
        }

        /* FSM tick */
        ElevFsm_Update(&g_elevB);

        /* Independent mode: if master offline, operate on cabin buttons only */
        if (!g_slaveOnline) {
            /* Already handled by FSM — cabin requests set by EXTI ISRs */
        }
    }
}
