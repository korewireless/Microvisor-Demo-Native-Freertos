/**
 *
 * Microvisor Native FreeRTOS Demo
 *
 * Copyright © 2024, KORE Wireless
 * Licence: MIT
 *
 */
#ifndef MCP9808_HEADER
#define MCP9808_HEADER


/*
 *  CONSTANTS
 */
// Sensor I2C address
#define MCP9808_ADDR                    0x18

// Register addresses
#define MCP9808_REG_CONFIG              0x01
#define MCP9808_REG_UPPER_TEMP          0x02
#define MCP9808_REG_LOWER_TEMP          0x03
#define MCP9808_REG_CRIT_TEMP           0x04
#define MCP9808_REG_AMBIENT_TEMP        0x05
#define MCP9808_REG_MANUF_ID            0x06
#define MCP9808_REG_DEVICE_ID           0x07

#define MCP9808_CONFIG_CLEAR_ALERT      0xDF
#define MCP9808_CONFIG_ENABLE_ALERT     0x08
#define MCP9808_CONFIG_ALERT_POL        0x02
#define MCP9808_CONFIG_ALERT_MODE       0x01

#define DEFAULT_TEMP_LOWER_LIMIT_C      10
#define DEFAULT_TEMP_UPPER_LIMIT_C      30
#define DEFAULT_TEMP_CRIT_LIMIT_C       50


#ifdef __cplusplus
extern "C" {
#endif


/*
 *  PROTOTYPES
 */
bool    MCP9808_init(void) ;
double  MCP9808_read_temp(void);
void    MCP9808_clear_alert(bool do_enable);
void    MCP9808_set_upper_limit(uint16_t upper_temp);
void    MCP9808_set_critical_limit(uint16_t critical_temp);
void    MCP9808_set_lower_limit(uint16_t lower_temp);
bool    MCP9808_get_alert_state(void);


#ifdef __cplusplus
}
#endif


#endif  // MCP9808_HEADER
