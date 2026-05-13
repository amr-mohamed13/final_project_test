//
// Created by AbdallahDarwish on 2026-03-19.
//

#include "Utils.h"

/* Calibrated for STM32F401 @ 16 MHz HSI, compiled at -O0.
 * Each empty loop iteration ≈ 5 CPU cycles.
 * 16,000,000 / 5 = 3,200,000 iterations → 1 second.
 * So 1 ms ≈ 3200 iterations.                          */
void delay_ms(uint32 ms) {
  volatile uint32 i;
  for (i = 0; i < ms * 3200U; i++) {
  }
}