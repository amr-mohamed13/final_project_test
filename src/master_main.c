/**
 * master_main.c — Master MCU Entry Point
 *
 * Elevator A + Dispatcher + SPI Master + UART Telemetry
 * STM32F401xE @ 16 MHz HSI
 */

#include "Dma.h"
#include "Exti.h"
#include "Gpio.h"
#include "Pwm.h"
#include "Rcc.h"
#include "Spi.h"
#include "Timer.h"
#include "Uart.h"
#include "dispatcher.h"
#include "elevator_fsm.h"
#include "system_defs.h"
#include "telemetry.h"

/* ================================================================== */
/*  GLOBAL VOLATILE FLAGS                                             */
/* ================================================================== */
volatile uint8 g_spiExchangeFlag = 0;
volatile uint8 g_telemetryFlag = 0;
volatile uint8 g_spiRxCompleteFlag = 0; /* unused on Master */
volatile uint8 g_spiTimeoutCount = 0;
volatile uint8 g_slaveOnline = 1;

/* ================================================================== */
/*  ELEVATOR STATES                                                   */
/* ================================================================== */
ElevatorState_t g_elevA;
ElevatorState_t g_elevB; /* Shadow of Slave state (updated via SPI) */

/* ================================================================== */
/*  TIMER CALLBACKS (periodic)                                        */
/* ================================================================== */
static void SpiTimerCallback(void) { g_spiExchangeFlag = 1; }

static void TelemetryTimerCallback(void) { g_telemetryFlag = 1; }

/* ================================================================== */
/*  SPI EXCHANGE LOGIC                                                */
/* ================================================================== */
static SpiPacket_t s_txPkt;
static SpiPacket_t s_rxPkt;

static void DoSpiExchange(void) {
  /* Build TX packet with Elevator A state + command for Slave */
  uint8 slaveCmd = SPI_CMD_NOP;
  uint8 slaveUpMask = 0, slaveDownMask = 0;
  if (Dispatcher_GetSlaveAssignments(&slaveUpMask, &slaveDownMask)) {
    slaveCmd = SPI_CMD_ASSIGN_FLOOR;
  }

  /* Emergency broadcast overrides all other commands */
  static uint8 s_prevEmergency = 0;
  if (g_elevA.emergencyStop) {
    slaveCmd = SPI_CMD_EMERGENCY_ALL;
    s_prevEmergency = 1;
  } else if (s_prevEmergency) {
    /* Emergency just cleared — tell slave to resume */
    slaveCmd = SPI_CMD_EMERGENCY_RELEASE;
    s_prevEmergency = 0;
  }

  /* Protect g_elevA from concurrent EXTI modifications during read */
  ENTER_CRITICAL();
  SpiPacket_Build(&s_txPkt, ELEV_A, &g_elevA, slaveCmd);
  EXIT_CRITICAL();

  if (slaveCmd == SPI_CMD_ASSIGN_FLOOR) {
    /* Pack UP (low nibble) and DOWN (high nibble) into requestMask.
     * Floors are 0–3, so 4 bits per direction fits perfectly. */
    s_txPkt.requestMask = (slaveUpMask & 0x0FU) | ((slaveDownMask & 0x0FU) << 4U);
    /* Recompute checksum AFTER overriding requestMask */
    s_txPkt.checksum = SpiPacket_CalcChecksum(&s_txPkt);
  }

  /* Exchange */
  uint8 result = Spi_MasterExchange((const uint8 *)&s_txPkt, (uint8 *)&s_rxPkt,
                                    SPI_FRAME_LEN);

  if (result != 0 || !SpiPacket_IsValid(&s_rxPkt)) {
    /* SPI failed — increment timeout counter */
    g_spiTimeoutCount++;
    if (g_spiTimeoutCount >= SPI_TIMEOUT_THRESHOLD) {
      g_slaveOnline = 0;
      /* Master assumes ALL hall calls */
      ENTER_CRITICAL();
      g_elevB.fsmState = ELEV_EMERGENCY;
      EXIT_CRITICAL();
    }
  } else {
    /* Update shadow of Slave state — protect from concurrent reads */
    ENTER_CRITICAL();
    g_spiTimeoutCount = 0;
    g_slaveOnline = 1;
    g_elevB.currentFloor = s_rxPkt.currentFloor;
    g_elevB.direction = s_rxPkt.direction;
    g_elevB.fsmState = s_rxPkt.state;
    EXIT_CRITICAL();
  }
}

/* ================================================================== */
/*  GPIO INITIALISATION                                               */
/* ================================================================== */
static void InitGpio(void) {
  /* --- SPI1 pins: AF5 --- */
  Gpio_Init(PIN_SPI_SCK_PORT, PIN_SPI_SCK_PIN, GPIO_AF, GPIO_PUSH_PULL);
  Gpio_SetAF(PIN_SPI_SCK_PORT, PIN_SPI_SCK_PIN, GPIO_AF5);
  Gpio_Init(PIN_SPI_MISO_PORT, PIN_SPI_MISO_PIN, GPIO_AF, GPIO_PUSH_PULL);
  Gpio_SetAF(PIN_SPI_MISO_PORT, PIN_SPI_MISO_PIN, GPIO_AF5);
  Gpio_Init(PIN_SPI_MOSI_PORT, PIN_SPI_MOSI_PIN, GPIO_AF, GPIO_PUSH_PULL);
  Gpio_SetAF(PIN_SPI_MOSI_PORT, PIN_SPI_MOSI_PIN, GPIO_AF5);

  /* CS = GPIO output, idle high */
  Gpio_Init(PIN_SPI_CS_PORT, PIN_SPI_CS_PIN, GPIO_OUTPUT, GPIO_PUSH_PULL);
  Gpio_WritePin(PIN_SPI_CS_PORT, PIN_SPI_CS_PIN, HIGH);

  /* --- UART TX: AF7 (USART2) --- */
  Gpio_Init(PIN_UART_TX_PORT, PIN_UART_TX_PIN, GPIO_AF, GPIO_PUSH_PULL);
  Gpio_SetAF(PIN_UART_TX_PORT, PIN_UART_TX_PIN, GPIO_AF7);

  /* --- Motor PWM: AF1 (TIM2_CH1) --- */
  Gpio_Init(PIN_MOTOR_PORT, PIN_MOTOR_PIN, GPIO_AF, GPIO_PUSH_PULL);
  Gpio_SetAF(PIN_MOTOR_PORT, PIN_MOTOR_PIN, GPIO_AF1);

  /* --- Cabin buttons: PB0-PB3, input pull-up --- */
  Gpio_Init(PIN_CABIN_PORT, PIN_CABIN_F1, GPIO_INPUT, GPIO_PULL_UP);
  Gpio_Init(PIN_CABIN_PORT, PIN_CABIN_F2, GPIO_INPUT, GPIO_PULL_UP);
  Gpio_Init(PIN_CABIN_PORT, PIN_CABIN_F3, GPIO_INPUT, GPIO_PULL_UP);
  Gpio_Init(PIN_CABIN_PORT, PIN_CABIN_F4, GPIO_INPUT, GPIO_PULL_UP);

  /* --- Emergency: PB4, input pull-up --- */
  Gpio_Init(PIN_EMERG_PORT, PIN_EMERG_PIN, GPIO_INPUT, GPIO_PULL_UP);

  /* --- Hallway call buttons: PB5-PB10, input pull-up --- */
  Gpio_Init(PIN_HALL_PORT, PIN_HALL_U1, GPIO_INPUT, GPIO_PULL_UP);
  Gpio_Init(PIN_HALL_PORT, PIN_HALL_D2, GPIO_INPUT, GPIO_PULL_UP);
  Gpio_Init(PIN_HALL_PORT, PIN_HALL_U2, GPIO_INPUT, GPIO_PULL_UP);
  Gpio_Init(PIN_HALL_PORT, PIN_HALL_D3, GPIO_INPUT, GPIO_PULL_UP);
  Gpio_Init(PIN_HALL_PORT, PIN_HALL_U3, GPIO_INPUT, GPIO_PULL_UP);
  Gpio_Init(PIN_HALL_PORT, PIN_HALL_D4, GPIO_INPUT, GPIO_PULL_UP);

  /* --- Position sensors: PC12-PC15, input pull-down --- */
  Gpio_Init(PIN_POS_PORT, PIN_POS_F1, GPIO_INPUT, GPIO_PULL_DOWN);
  Gpio_Init(PIN_POS_PORT, PIN_POS_F2, GPIO_INPUT, GPIO_PULL_DOWN);
  Gpio_Init(PIN_POS_PORT, PIN_POS_F3, GPIO_INPUT, GPIO_PULL_DOWN);
  Gpio_Init(PIN_POS_PORT, PIN_POS_F4, GPIO_INPUT, GPIO_PULL_DOWN);
}

/* ================================================================== */
/*  EXTI INITIALISATION                                               */
/* ================================================================== */
static void InitExti(void) {
  /* Cabin buttons — PB0-PB3, falling edge, priority 4 */
  Exti_Init(EXTI_PORT_B, PIN_CABIN_F1, EXTI_FALLING, 4U);
  Exti_Init(EXTI_PORT_B, PIN_CABIN_F2, EXTI_FALLING, 4U);
  Exti_Init(EXTI_PORT_B, PIN_CABIN_F3, EXTI_FALLING, 4U);
  Exti_Init(EXTI_PORT_B, PIN_CABIN_F4, EXTI_FALLING, 4U);

  /* Emergency — PB4, falling edge only (toggle via ISR), priority 0 (HIGHEST) */
  Exti_Init(EXTI_PORT_B, PIN_EMERG_PIN, EXTI_FALLING, 0U);

  /* Hallway calls — PB5-PB10, falling edge, priority 3 */
  Exti_Init(EXTI_PORT_B, PIN_HALL_U1, EXTI_FALLING, 3U);
  Exti_Init(EXTI_PORT_B, PIN_HALL_D2, EXTI_FALLING, 3U);
  Exti_Init(EXTI_PORT_B, PIN_HALL_U2, EXTI_FALLING, 3U);
  Exti_Init(EXTI_PORT_B, PIN_HALL_D3, EXTI_FALLING, 3U);
  Exti_Init(EXTI_PORT_B, PIN_HALL_U3, EXTI_FALLING, 3U);
  Exti_Init(EXTI_PORT_B, PIN_HALL_D4, EXTI_FALLING, 3U);

  /* Position sensors — PC12-PC15, rising edge, priority 2 */
  Exti_Init(EXTI_PORT_C, PIN_POS_F1, EXTI_RISING, 2U);
  Exti_Init(EXTI_PORT_C, PIN_POS_F2, EXTI_RISING, 2U);
  Exti_Init(EXTI_PORT_C, PIN_POS_F3, EXTI_RISING, 2U);
  Exti_Init(EXTI_PORT_C, PIN_POS_F4, EXTI_RISING, 2U);
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
  Rcc_Enable(RCC_TIM4);
  Rcc_Enable(RCC_TIM5);
  Rcc_Enable(RCC_SPI1);
  Rcc_Enable(RCC_USART2);
  Rcc_Enable(RCC_DMA1);

  /* 2. GPIO */
  InitGpio();

  /* 3. EXTI */
  InitExti();

  /* 4. SPI Master */
  Spi_InitMaster();

  /* 5. UART + DMA */
  Dma_Init();
  Uart_Init(115200);

  /* 6. Motor PWM (TIM2 CH1) */
  Pwm_Init(MOTOR_TIMER, MOTOR_CHANNEL, MOTOR_PWM_PSC, MOTOR_PWM_ARR);
  Pwm_Start(MOTOR_TIMER, MOTOR_CHANNEL);

  /* 7. FSM + Dispatcher init */
  ElevFsm_Init(&g_elevA);
  ElevFsm_Init(&g_elevB);
  Dispatcher_Init(&g_elevA, &g_elevB);

  /* 8. Start periodic timers */
  /* TIM3: SPI exchange every 50ms  (PSC=15999 → 1kHz tick, ARR=49 → 50ms) */
  Timer_StartPeriodic(SPI_TIMER, 15999U, 49U, SpiTimerCallback);
  /* TIM4: Telemetry every 500ms   (PSC=15999 → 1kHz tick, ARR=499 → 500ms) */
  Timer_StartPeriodic(TELEMETRY_TIMER, 15999U, 499U, TelemetryTimerCallback);

  /* ============ SUPERLOOP ============ */
  while (1) {
    /* SPI exchange (every 50ms) */
    if (g_spiExchangeFlag) {
      g_spiExchangeFlag = 0;
      Dispatcher_Run();    /* Must run BEFORE exchange so assignments are ready */
      DoSpiExchange();
    }

    /* FSM tick */
    ElevFsm_Update(&g_elevA);

    /* Telemetry (every 500ms) */
    if (g_telemetryFlag) {
      g_telemetryFlag = 0;
      Telemetry_Send(&g_elevA, &g_elevB);
    }
  }
}
