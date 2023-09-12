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
#include <semphr.h>
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
#include "stm32u585xx.h"
#include "stm32u5xx_hal.h"
#include "stm32u5xx_it.h"
#include "mv_syscalls.h"
// Application
#include "i2c.h"
#include "mcp9808.h"
#include "logging.h"


/*
 * CONSTANTS
 */
#define     SENSOR_READ_INTERVAL_MS     10000
#define     LED_FLASH_INTERVAL_MS       250
#define     ALERT_DISPLAY_PERIOD_MS     20000

#define     SENSOR_TASK_WAIT_TICKS     20

#define     LED_GPIO_PORT               GPIOA
#define     LED_GPIO_PIN                GPIO_PIN_5

#define     MCP_GPIO_PORT               GPIOB
#define     MCP_INT_PIN                 GPIO_PIN_11
#define     MCP_INT_IRQ                 EXTI11_IRQn

#define     I2C_GPIO_PORT               GPIOB
#define     I2C_SDA_PIN_9               GPIO_PIN_9
#define     I2C_SCL_PIN_6               GPIO_PIN_6

#define     TEMP_LOWER_LIMIT_C          10
#define     TEMP_UPPER_LIMIT_C          30
#define     TEMP_CRIT_LIMIT_C           50


#ifdef __cplusplus
extern "C" {
#endif


/*
 * PROTOTYPES
 */
void server_log(char* format_string, ...);
void server_error(char* format_string, ...);
// Interrupt for alert pin
void EXTI11_IRQHandler(void);


#ifdef __cplusplus
}
#endif


#endif /* MAIN_H */

