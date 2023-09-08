/**
 *
 * Microvisor Native FreeRTOS Demo
 * Version 1.0.0
 * Copyright © 2023, KORE Wireless
 * Licence: MIT
 *
 */
#ifndef MAIN_H
#define MAIN_H

/*
 * IMPORTS
 */
// FreeRTOS
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <timers.h>
// C
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <stdarg.h>
#include <stdbool.h>
#include <assert.h>
// Microvisor
#include "mv_syscalls.h"
#include "stm32u5xx_hal.h"
// Application
#include "i2c.h"
#include "mcp9808.h"
#include "logging.h"


/*
 * CONSTANTS
 */
#define     DEBUG_SENSOR_PAUSE_MS       10000
#define     DEBUG_LED_PAUSE_MS          250
#define     ALERT_DISPLAY_PERIOD_MS     10000

#define     SENSOR_TASK_DELAY_TICKS     20

#define     LED_GPIO_PORT               GPIOA
#define     LED_GPIO_PIN                GPIO_PIN_5

#define     MCP_GPIO_PORT               GPIOD
#define     MCP_INT_PIN                 GPIO_PIN_8
#define     MCP_INT_IRQ                 EXTI8_IRQn

#define     TEMP_LOWER_LIMIT_C          10
#define     TEMP_UPPER_LIMIT_C          28
#define     TEMP_CRIT_LIMIT_C           50


#ifdef __cplusplus
extern "C" {
#endif


/*
 * PROTOTYPES
 */
void server_log(char* format_string, ...);
void server_error(char* format_string, ...);


#ifdef __cplusplus
}
#endif


#endif /* MAIN_H */

