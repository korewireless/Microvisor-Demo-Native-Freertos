/**
 *
 * Microvisor Native FreeRTOS Demo
 * Version 1.0.0
 * Copyright © 2023, KORE Wireless
 * Licence: MIT
 *
 */
#include "main.h"


/*
 * STATIC PROTOTYPES
 */
static bool I2C_check(uint8_t addr);


/*
 * GLOBALS
 */
I2C_HandleTypeDef   i2c;


/**
 * @brief Initialize STM32U585 I2C1.
 *        NOTE Pins are configured in the HAL callback function
 *             `HAL_I2C_MspInit()`.
 *
 * @returns `true` if initialization succeeded, otherwise `false`.
 */
bool I2C_init(void) {

    // I2C1 pins are:
    //   SDA -> PB9
    //   SCL -> PB6
    i2c.Instance              = I2C1;
    i2c.Init.Timing           = 0x00C01F67;  // 400kHz, FROM ST SAMPLE
    i2c.Init.AddressingMode   = I2C_ADDRESSINGMODE_7BIT;
    i2c.Init.DualAddressMode  = I2C_DUALADDRESS_DISABLE;
    i2c.Init.OwnAddress1      = 0x00;
    i2c.Init.OwnAddress2      = 0x00;
    i2c.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    i2c.Init.GeneralCallMode  = I2C_GENERALCALL_DISABLE;
    i2c.Init.NoStretchMode    = I2C_NOSTRETCH_ENABLE;

    // Initialize the I2C itself with the i2c handle
    if (HAL_I2C_Init(&i2c) != HAL_OK) {
        server_error("I2C initialization failed");
        return false;
    }

    // Check MCP9808's presence
    return I2C_check(MCP9808_ADDR);
}


/**
 * @brief Check for presence of a known device by its I2C address.
 *
 * @param target_address: The device's address.
 *
 * @returns `true` if the device is present, otherwise `false`.
 */
static bool I2C_check(uint8_t target_address) {

    uint8_t timeout_count = 0;

    while(true) {
        HAL_StatusTypeDef status = HAL_I2C_IsDeviceReady(&i2c, target_address << 1, 1, 100);
        if (status == HAL_OK) {
            return true;
        } else {
            uint32_t err = HAL_I2C_GetError(&i2c);
            server_error("HAL_I2C_IsDeviceReady() : %i", status);
            server_error("HAL_I2C_GetError():       %li", err);
        }

        HAL_Delay(500);
        timeout_count++;
        if (timeout_count > 10) break;
    }

    // Flash the LED ten times on device not ready
    for (uint8_t i = 0 ; i < 10 ; ++i) {
        HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_GPIO_PIN);
        HAL_Delay(100);
    }

    return false;
}


/**
 * @brief HAL-called function to complete I2C configuration.
 *        Configure your I2C pins here.
 *        This is called by `HAL_I2C_Init()`.
 *
 * @param i2c: A HAL I2C_HandleTypeDef pointer to the I2C instance.
 */
void HAL_I2C_MspInit(I2C_HandleTypeDef *i2c) {

    // Configure U5 peripheral clock
    RCC_PeriphCLKInitTypeDef PeriphClkInit = { 0 };
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C1;
    PeriphClkInit.I2c1ClockSelection   = RCC_I2C1CLKSOURCE_PCLK1;

    // Initialize U5 peripheral clock
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
        server_error("HAL_RCCEx_PeriphCLKConfig() failed");
        return;
    }

    // Enable the I2C GPIO interface clock
    __HAL_RCC_GPIOB_CLK_ENABLE();

    // Configure the GPIO pins for I2C
    // Pin PB6 - SCL
    // Pin PB9 - SDA
    GPIO_InitTypeDef i2c_config = { 0 };
    i2c_config.Pin       = I2C_SCL_PIN_6 | I2C_SDA_PIN_9;
    i2c_config.Mode      = GPIO_MODE_AF_OD;
    i2c_config.Pull      = GPIO_NOPULL;
    i2c_config.Speed     = GPIO_SPEED_FREQ_LOW;
    i2c_config.Alternate = GPIO_AF4_I2C1;

    // Initialize the pins with the setup data
    HAL_GPIO_Init(I2C_GPIO_PORT, &i2c_config);

    // Enable the I2C1 clock
    __HAL_RCC_I2C1_CLK_ENABLE();
}
