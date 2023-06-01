/**
 *
 * Microvisor Native FreeRTOS Demo
 * Version 1.0.0
 * Copyright © 2023, KORE Wireless
 * Licence: MIT
 *
 */
#include "main.h"
#include "app_version.h"


/*
 * STATIC PROTOTYPES
 */
static void         system_clock_config(void);
static void         GPIO_init(void);
static void         task_led(void *argument);
static void         task_sensor(void *argument);
static void         task_alert(void* argument);
static void         set_alert_timer(void);
static void         timer_fired_callback(TimerHandle_t timer);
static inline void  isr_worker(void);
static void         log_device_info(void);


/*
 * GLOBALS
 */
// FreeRTOS Task handles
TaskHandle_t handle_task_sensor = NULL;
TaskHandle_t handle_task_led = NULL;
TaskHandle_t handle_task_alert = NULL;

// FreeRTOS Timers
TimerHandle_t alert_timer = NULL;

// I2C-related values (defined in `i2c.c`)
extern I2C_HandleTypeDef i2c;


/**
 *  Theses variables may be changed by interrupt handler code,
 *  so we mark them as `volatile` to ensure compiler optimization
 *  doesn't render them immutable at runtime
 */
static volatile bool    use_i2c = false;
static volatile double  current_temp = 0.0;
static volatile bool    got_mcp9808 = false;


/**
 *  @brief The application entry point.
 */
int main(void) {

    // Initialise the STM32U5 HAL
    HAL_Init();

    // Configure the system clock
    system_clock_config();

    // Log the device ID and app details
    log_device_info();

    // Initialise hardware: the LED and alert pins,
    // and the I2C bus to which the MCP9808 is connected.
    GPIO_init();
    use_i2c = I2C_init();
    if (use_i2c) got_mcp9808 = MCP9808_init();

    // Prep the MCP9808 temperature sensor (if present)
    if (got_mcp9808) {
        // Set the lower, upper and critical temperature values
        MCP9808_set_lower_limit(TEMP_LOWER_LIMIT_C);
        MCP9808_set_upper_limit(TEMP_UPPER_LIMIT_C);
        MCP9808_set_critical_limit(TEMP_CRIT_LIMIT_C);

        // Get a temperature reading
        current_temp = MCP9808_read_temp();
        server_log("MCP9808 ready");
    } else {
        server_error("MCP9808 not ready");
    }

    // Set up two FreeRTOS tasks
    // NOTE Argument #3 is the task stack size in words not bytes, ie. 512 -> 2048 bytes
    //      Task stack sizes are allocated in the FreeRTOS heap, set in `FreeRTOSConfig.h`
    BaseType_t status_task_led = xTaskCreate(task_led,
                                              "LED_TASK",
                                              1024,
                                              NULL,
                                              1,
                                              &handle_task_led);
    BaseType_t status_task_sensor = xTaskCreate(task_sensor,
                                              "WORK_TASK",
                                              2048,
                                              NULL,
                                              1,
                                              &handle_task_sensor);
    BaseType_t status_task_alert = xTaskCreate(task_alert,
                                              "ALERT_TASK",
                                              1024,
                                              NULL,
                                              1,
                                              &handle_task_alert);

    if (status_task_led == pdPASS && status_task_sensor == pdPASS && status_task_alert == pdPASS) {
        // Start the scheduler
        server_log("Starting FreeRTOS scheduler");
        vTaskStartScheduler();
    }

    // We should never get here as control is now taken by the scheduler
    server_error("Insufficient RAM to start default tasks");

    while (1) {
        // NOP
    }
}


/**
 * @brief Get the Microvisor clock reading.
 *
 * @retval The clock value.
 */
uint32_t SECURE_SystemCoreClockUpdate() {

    uint32_t clock = 0;
    mvGetHClk(&clock);
    return clock;
}


/**
 * @brief System clock configuration.
 */
static void system_clock_config(void) {

    SystemCoreClockUpdate();
    HAL_InitTick(TICK_INT_PRIORITY);
}


/**
 * @brief GPIO initialization function.
 *        This configures pin PA5, which is wired to the Nucleo's
 *        USER LED, and PF3, which is triggers an interrupt when it
 *        goes high (in response to alert signal from MCP9808).
 */
static void GPIO_init(void) {

    // Enable the clock for GPIO ports A (USER LED), F (MCP9808 INT PIN)
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();

    // Clear the LED
    HAL_GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MCP_GPIO_PORT, MCP_INT_PIN, GPIO_PIN_RESET);

    // Configure GPIO pin for the Nucleo's USER LED: PA5
    GPIO_InitTypeDef led_init_data = {0};
    led_init_data.Pin   = LED_GPIO_PIN;
    led_init_data.Mode  = GPIO_MODE_OUTPUT_PP;
    led_init_data.Pull  = GPIO_PULLUP;
    led_init_data.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(LED_GPIO_PORT, &led_init_data);

    // Configure GPIO pin for the MCP9808 interrupt: PF3
    GPIO_InitTypeDef mcp_init_data = {0};
    mcp_init_data.Pin   = MCP_INT_PIN;
    mcp_init_data.Mode  = GPIO_MODE_IT_RISING;
    mcp_init_data.Pull  = GPIO_NOPULL;
    mcp_init_data.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(MCP_GPIO_PORT, &mcp_init_data);

    // Set up the NVIC to process interrupts
    HAL_NVIC_SetPriority(MCP_INT_IRQ, 3, 0);
    HAL_NVIC_EnableIRQ(MCP_INT_IRQ);
}


/**
 * @brief  Function implementing the LED flasher task.
 *         NOTE Currently does nothing.
 *
 * @param  argument: Not used
 */
static void task_led(void *argument) {

    // Get the pause period in ticks from a millisecond value
    const TickType_t led_pause_ticks = pdMS_TO_TICKS(DEBUG_LED_PAUSE_MS);

    while(1) {
        // Toggle the NDB's USER LED
        // HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_GPIO_PIN);

        // Yield execution for a period
        vTaskDelay(led_pause_ticks);
    }
}


/**
 * @brief  Function implementing the MCP9808 temperature read task.
 *
 * @param  argument: Not used
 */
static void task_sensor(void *argument) {

    // Get the pause period in ticks from a millisecond value
    const TickType_t ping_pause_ticks = pdMS_TO_TICKS(DEBUG_PING_PAUSE_MS);

    while(1) {
        // Output the current reading
        if (got_mcp9808) {
            current_temp = MCP9808_read_temp();
        }

        server_log("Current temperature: %.2f°C", current_temp);

        // Yield execution for a period
        vTaskDelay(ping_pause_ticks);
    }
}


/**
 * @brief  Function implementing the alert watcher task.
 *
 * @param  argument: Not used
 */
static void task_alert(void* argument) {

    while (true) {
        // Block until a notification arrives
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // Show the IRQ was hit
        HAL_GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_PIN_SET);
        if (got_mcp9808) MCP9808_clear_alert(false);
        server_log("IRQ detected");

        // Set and start a timer to clear the alert
        set_alert_timer();
    }
}


/**
 * @brief Show basic device info.
 */
static void log_device_info(void) {

    uint8_t dev_id[35] = { 0 };
    mvGetDeviceId(dev_id, 34);
    server_log("Device: %s", dev_id);
    server_log("   App: %s %s", APP_NAME, APP_VERSION);
    server_log(" Build: %i", BUILD_NUM);
}


/**
 * @brief Set and start a timer to clear the current alert.
 */
static void set_alert_timer(void) {

    // Set up and start a FreeRTOS timer
    alert_timer = xTimerCreate("ALERT_TIMER",
                               pdMS_TO_TICKS(ALERT_DISPLAY_PERIOD_MS),
                               pdFALSE,
                               (void*)0,
                               timer_fired_callback);
    if (alert_timer != NULL) xTimerStart(alert_timer, SENSOR_TASK_DELAY_TICKS);
}


/**
 * @brief Callback actioned when the post IRQ timer fires.
 *
 * @param timer: The triggering timer.
 */
static void timer_fired_callback(TimerHandle_t timer) {

    // Check whether the alert condition has passed
    // NOTE The MCP980 does not signal this on the ALERT pin
    if (current_temp < (double)TEMP_UPPER_LIMIT_C) {
        // Clear the LED
        HAL_GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_PIN_RESET);

        // Clear the timer
        alert_timer = NULL;

        // Reset the MCP9808 alert mechanism
        MCP9808_clear_alert(true);

        // The alert IRQ is disabled at this point, so reenable it
        // NOTE This has to come after the previous line, or it
        //      will trip immediately!
        HAL_NVIC_EnableIRQ(MCP_INT_IRQ);
    } else {
        // Temperature still too high -- restart the timer
        set_alert_timer();
    }
}


/**
 * @brief Interrupt handler as specified by the STM32U5 HAL.
 */
void EXTI3_IRQHandler(void) {

    HAL_GPIO_EXTI_IRQHandler(MCP_INT_PIN);
}


/**
 * @brief Common ISR code.
 */
static inline void isr_worker(void) {

    // Signal the alert clearance task
    static BaseType_t higher_priority_task_woken = pdFALSE;
    vTaskNotifyGiveFromISR(handle_task_alert, &higher_priority_task_woken);

    // Disable the IRQ to prevent repeated alerts
    // (we will re-enable the IRQ when the alert is over)
    HAL_NVIC_DisableIRQ(MCP_INT_IRQ);

    // Exit to FreeRTOS context switch if necessary
    portYIELD_FROM_ISR(higher_priority_task_woken);
}


/**
 * @brief IRQ handler as specified in HAL doc.
 *
 * @param GPIO_Pin The pin the triggered the IRQ
 */
void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin) {

    isr_worker();
}


/**
 * @brief IRQ handler as specified in HAL doc.
 *
 * @param GPIO_Pin The pin the triggered the IRQ
 */
void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin) {

    isr_worker();
}
