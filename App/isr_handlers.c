/**
 * isr_handlers.c — EXTI Interrupt Service Routines
 *
 * Handles cabin buttons, emergency stop, hallway calls (Master),
 * and position sensors for both MCUs.
 * All handlers set volatile flags — processing occurs in the main loop.
 */

#include "system_defs.h"
#include "elevator_fsm.h"
#include "Exti.h"
#include "Gpio.h"

#ifdef MASTER_MCU
#include "dispatcher.h"
#endif

/* ================================================================== */
/*  External state (defined in master_main.c or slave_main.c)         */
/* ================================================================== */
#ifdef MASTER_MCU
extern ElevatorState_t g_elevA;
#define LOCAL_ELEV g_elevA
#else
extern ElevatorState_t g_elevB;
#define LOCAL_ELEV g_elevB
#endif

/* ================================================================== */
/*  CABIN BUTTONS  (PB0 = F1, PB1 = F2, PB2 = F3, PB3 = F4)         */
/* ================================================================== */

void EXTI0_IRQHandler(void) {
    Exti_ClearPending(PIN_CABIN_F1);
    ElevFsm_AddCabinRequest(&LOCAL_ELEV, 0);
}

void EXTI1_IRQHandler(void) {
    Exti_ClearPending(PIN_CABIN_F2);
    ElevFsm_AddCabinRequest(&LOCAL_ELEV, 1);
}

void EXTI2_IRQHandler(void) {
    Exti_ClearPending(PIN_CABIN_F3);
    ElevFsm_AddCabinRequest(&LOCAL_ELEV, 2);
}

void EXTI3_IRQHandler(void) {
    Exti_ClearPending(PIN_CABIN_F4);
    ElevFsm_AddCabinRequest(&LOCAL_ELEV, 3);
}

/* ================================================================== */
/*  EMERGENCY STOP  (PB4) — Highest NVIC priority                    */
/* ================================================================== */

void EXTI4_IRQHandler(void) {
    Exti_ClearPending(PIN_EMERG_PIN);
    /* Toggle emergency state on every press (momentary tactile button) */
    if (LOCAL_ELEV.emergencyStop == 0) {
        ElevFsm_EmergencyStop(&LOCAL_ELEV);
    } else {
        ElevFsm_EmergencyRelease(&LOCAL_ELEV);
    }
}

/* ================================================================== */
/*  HALLWAY CALLS (PB5-PB9)  — Master only                           */
/*  POSITION SENSORS (PC12-PC15) — shared handler EXTI15_10          */
/* ================================================================== */

void EXTI9_5_IRQHandler(void) {
#ifdef MASTER_MCU
    /* PB5 = Hall U1 (Floor 0 UP) */
    if (Gpio_ReadPin(PIN_HALL_PORT, PIN_HALL_U1) == LOW) {
        Exti_ClearPending(PIN_HALL_U1);
        Dispatcher_RegisterHallCall(0, DIR_UP);
    }
    /* PB6 = Hall D2 (Floor 1 DOWN) */
    if (Gpio_ReadPin(PIN_HALL_PORT, PIN_HALL_D2) == LOW) {
        Exti_ClearPending(PIN_HALL_D2);
        Dispatcher_RegisterHallCall(1, DIR_DOWN);
    }
    /* PB7 = Hall U2 (Floor 1 UP) */
    if (Gpio_ReadPin(PIN_HALL_PORT, PIN_HALL_U2) == LOW) {
        Exti_ClearPending(PIN_HALL_U2);
        Dispatcher_RegisterHallCall(1, DIR_UP);
    }
    /* PB8 = Hall D3 (Floor 2 DOWN) */
    if (Gpio_ReadPin(PIN_HALL_PORT, PIN_HALL_D3) == LOW) {
        Exti_ClearPending(PIN_HALL_D3);
        Dispatcher_RegisterHallCall(2, DIR_DOWN);
    }
    /* PB9 = Hall U3 (Floor 2 UP) */
    if (Gpio_ReadPin(PIN_HALL_PORT, PIN_HALL_U3) == LOW) {
        Exti_ClearPending(PIN_HALL_U3);
        Dispatcher_RegisterHallCall(2, DIR_UP);
    }
#else
    /* Slave has no hall buttons on these lines — clear all pending */
    Exti_ClearPending(5);
    Exti_ClearPending(6);
    Exti_ClearPending(7);
    Exti_ClearPending(8);
    Exti_ClearPending(9);
#endif
}

void EXTI15_10_IRQHandler(void) {
#ifdef MASTER_MCU
    /* PB10 = Hall D4 (Floor 3 DOWN) */
    if (Gpio_ReadPin(PIN_HALL_PORT, PIN_HALL_D4) == LOW) {
        Exti_ClearPending(PIN_HALL_D4);
        Dispatcher_RegisterHallCall(3, DIR_DOWN);
    }
#endif

    /* PC12 = Position sensor Floor 0 */
    if (Gpio_ReadPin(PIN_POS_PORT, PIN_POS_F1) == HIGH) {
        Exti_ClearPending(PIN_POS_F1);
        LOCAL_ELEV.floorArrived = 1;
        LOCAL_ELEV.arrivedFloor = 0;
    }
    /* PC13 = Position sensor Floor 1 */
    if (Gpio_ReadPin(PIN_POS_PORT, PIN_POS_F2) == HIGH) {
        Exti_ClearPending(PIN_POS_F2);
        LOCAL_ELEV.floorArrived = 1;
        LOCAL_ELEV.arrivedFloor = 1;
    }
    /* PC14 = Position sensor Floor 2 */
    if (Gpio_ReadPin(PIN_POS_PORT, PIN_POS_F3) == HIGH) {
        Exti_ClearPending(PIN_POS_F3);
        LOCAL_ELEV.floorArrived = 1;
        LOCAL_ELEV.arrivedFloor = 2;
    }
    /* PC15 = Position sensor Floor 3 */
    if (Gpio_ReadPin(PIN_POS_PORT, PIN_POS_F4) == HIGH) {
        Exti_ClearPending(PIN_POS_F4);
        LOCAL_ELEV.floorArrived = 1;
        LOCAL_ELEV.arrivedFloor = 3;
    }
}
