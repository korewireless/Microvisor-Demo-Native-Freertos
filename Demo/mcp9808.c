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
static void     MCP9808_set_temp_limit(uint8_t temp_register, uint16_t temp);
static double   MCP9808_get_temp(uint8_t* data);


/*
 * GLOBALS
 */
// I2C-related values (defined in `i2c.c`)
extern I2C_HandleTypeDef i2c;

uint16_t    limit_critical = DEFAULT_TEMP_LOWER_LIMIT_C;
uint16_t    limit_lower = DEFAULT_TEMP_UPPER_LIMIT_C;;
uint16_t    limit_upper = DEFAULT_TEMP_CRIT_LIMIT_C;


/**
 *  @brief  Check the device is connected and operational.
 *
 *  @returns `true` if we can read values and they are right,
 *           otherwise `false`.
 */
bool MCP9808_init(void) {

    // Prep data storage buffers
    uint8_t mid_data[2] = {0};
    uint8_t did_data[2] = {0};

    // Read bytes from the sensor: MID...
    uint8_t cmd = MCP9808_REG_MANUF_ID;
    HAL_I2C_Master_Transmit(&i2c, MCP9808_ADDR << 1, &cmd,      1, 100);
    HAL_I2C_Master_Receive(&i2c,  MCP9808_ADDR << 1, mid_data, 2, 100);

    // ...DID
    cmd = MCP9808_REG_DEVICE_ID;
    HAL_I2C_Master_Transmit(&i2c, MCP9808_ADDR << 1, &cmd,      1, 100);
    HAL_I2C_Master_Receive(&i2c,  MCP9808_ADDR << 1, did_data, 2, 100);

    // Bytes to integers
    const uint16_t mid_value = (mid_data[0] << 8) | mid_data[1];
    const uint16_t did_value = (did_data[0] << 8) | did_data[1];
    //server_log("MCP9808 Manufacturer ID: 0x%04x, Device ID: 0x%04x", mid_value, did_value);

    // Return false on data error
    if (mid_value != 0x0054 || did_value != 0x0400) return false;

    // Clear and enable the alert pin
    MCP9808_clear_alert(true);
    return true;
}


/**
 *  @brief  Check the device is connected and operational.
 *
 *  @returns `true` if the sensor is correct, otherwise `false`.
 */
double MCP9808_read_temp(void) {

    // Read sensor and return its value in degrees celsius.
    uint8_t temp_data[2] = { 0x06, 0x30 };
    uint8_t cmd = MCP9808_REG_AMBIENT_TEMP;
    HAL_I2C_Master_Transmit(&i2c, MCP9808_ADDR << 1, &cmd, 1, 200);
    HAL_StatusTypeDef result = HAL_I2C_Master_Receive(&i2c, MCP9808_ADDR << 1, temp_data, 2, 500);

    // Check for a read error -- if there is one, return its code
    const uint32_t temp_raw = (temp_data[0] << 8) | temp_data[1];
    if (temp_raw == 0x630) return (double)result;

    // Scale and convert to signed value.
    return MCP9808_get_temp(temp_data);
}


/**
 * @brief Clear the sensor's alert flag, CONFIG bit 5.
 *        Optionally, enable the alert first.
 *
 * @param do_enable: Set to `true` to enable the alert.
 */
void MCP9808_clear_alert(bool do_enable) {

    // Read the current reg value
    uint8_t config_data[3] = {0};
    uint8_t reg = MCP9808_REG_CONFIG;
    HAL_I2C_Master_Transmit(&i2c, MCP9808_ADDR << 1, &reg, 1, 200);
    HAL_I2C_Master_Receive(&i2c, MCP9808_ADDR << 1, &config_data[1], 2, 500);

    // Set LSB bit 5 to clear the interrupt, and write it back
    config_data[0] = reg;
    config_data[2] = 0x23;

    if (do_enable) {
        config_data[2] |= MCP9808_CONFIG_ENABLE_ALRT;
    }

    // Write config data back with changes
    HAL_I2C_Master_Transmit(&i2c, MCP9808_ADDR << 1, config_data, 3, 200);

    // Read it back to apply?
    uint8_t check_data[2] = {0};
    HAL_I2C_Master_Transmit(&i2c, MCP9808_ADDR << 1, &reg, 1, 200);
    HAL_I2C_Master_Receive(&i2c, MCP9808_ADDR << 1, check_data, 2, 500);

    // Check the two values: READ LSB == WRITE & 0xDF
    if ((config_data[2] & 0xDF) != check_data[1]) {
        server_error("MCP9809 alert config mismatch. SET: %02x READ: %02x\n",  config_data[2],  check_data[1]);
    }
}


/**
 * @brief Set the sensor upper threshold temperature.
 *
 * @param upper_temp: The target temperature.
 */
void MCP9808_set_upper_limit(uint16_t upper_temp) {
    limit_upper = upper_temp;
    MCP9808_set_temp_limit(MCP9808_REG_UPPER_TEMP, upper_temp);
}


/**
 * @brief Set the sensor critical threshold temperature.
 *
 * @param critical_temp: The target temperature.
 */
void MCP9808_set_critical_limit(uint16_t critical_temp) {
    limit_critical = critical_temp;
    MCP9808_set_temp_limit(MCP9808_REG_CRIT_TEMP, critical_temp);
}


/**
 * @brief Set the sensor lower threshold temperature.
 *
 * @param lower_temp: The target temperature.
 */
void MCP9808_set_lower_limit(uint16_t lower_temp) {
    limit_lower = lower_temp;
    MCP9808_set_temp_limit(MCP9808_REG_LOWER_TEMP, lower_temp);
}


/**
 * @brief Set a sensor threshold temperature.
 *
 * @param temp_register: The target register:
 *                       MCP9808_REG_LOWER_TEMP
 *                       MCP9808_REG_UPPER_TEMP
 *                       MCP9808_REG_CRIT_TEMP.
 * @param temp:          The temperature (as an integer)
 */
static void MCP9808_set_temp_limit(uint8_t temp_register, uint16_t temp) {

    temp &= 127;
    temp = (temp << 4);
    uint8_t data[3] = {temp_register};
    data[1] = (temp & 0xFF00) >> 8;
    data[2] = temp & 0xFF;
    HAL_I2C_Master_Transmit(&i2c, MCP9808_ADDR << 1, data, 3, 200);
}


/**
 * @brief Calculate the temperature.
 *
 * @retval The temperature in Celsius.
 */
static double MCP9808_get_temp(uint8_t* data) {

    const uint32_t temp_raw = (data[0] << 8) | data[1];
    double temp_cel = (temp_raw & 0x0FFF) / 16.0;
    if (temp_raw & 0x1000) temp_cel = 256.0 - temp_cel;
    return temp_cel;
}