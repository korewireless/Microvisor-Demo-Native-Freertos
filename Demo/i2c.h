/**
 *
 * Microvisor Native FreeRTOS Demo
 * Version 1.0.0
 * Copyright © 2023, KORE Wireless
 * Licence: MIT
 *
 */
#ifndef _I2C_HEADER_
#define _I2C_HEADER_


/*
 * CONSTANTS
 */
#define     I2C_GPIO_PORT           GPIOB


#ifdef __cplusplus
extern "C" {
#endif


/*
 * PROTOTYPES
 */
bool I2C_init(void);


#ifdef __cplusplus
}
#endif


#endif  // _I2C_HEADER_
