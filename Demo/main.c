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
static void         init_gpio(void);
static void         task_led(void *argument);
static void         task_sensor(void *argument);
static void         task_alert(void* argument);
static void         set_alert_timer(void);
static void         timer_fired_callback(TimerHandle_t timer);
static void         log_device_info(void);


/*
 * GLOBALS
 */
// FreeRTOS Task handles
TaskHandle_t handle_task_sensor = NULL;
TaskHandle_t handle_task_led = NULL;
TaskHandle_t handle_task_alert = NULL;

// I2C-related values (defined in `i2c.c`)
extern I2C_HandleTypeDef i2c;

static bool    use_i2c = false;
static bool    got_mcp9808 = false;

/**
 *  Theses variables may be changed by interrupt handler code,
 *  so we mark them as `volatile` to ensure compiler optimization
 *  doesn't render them immutable at runtime
 */
static volatile bool    alert_fired = false;
static volatile double  current_temp = 0.0;
// FreeRTOS Timers
volatile TimerHandle_t alert_timer = NULL;


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
    init_gpio();
    use_i2c = I2C_init();
    if (use_i2c) got_mcp9808 = MCP9808_init();

    // Prep the MCP9808 temperature sensor (if present)
    if (got_mcp9808) {
        // Set the lower, upper and critical temperature values
        MCP9808_set_lower_limit(TEMP_LOWER_LIMIT_C);
        MCP9808_set_upper_limit(TEMP_UPPER_LIMIT_C);
        MCP9808_set_critical_limit(TEMP_CRIT_LIMIT_C);
        // And enable alerts (off by default)
        MCP9808_clear_alert(true);

        // Get a temperature reading
        current_temp = MCP9808_read_temp();
    } else {
        server_error("MCP9808 not ready");
    }

    // Set up two FreeRTOS tasks
    // NOTE Argument #3 is the task stack size in words not bytes, ie. 512 -> 2048 bytes
    //      Task stack sizes are allocated in the FreeRTOS heap, set in `FreeRTOSConfig.h`
    BaseType_t status_task_led = xTaskCreate(task_led, "LED_TASK", 1024, NULL, 1, &handle_task_led);
    BaseType_t status_task_sensor = xTaskCreate(task_sensor, "WORK_TASK", 2048, NULL, 1, &handle_task_sensor);
    BaseType_t status_task_alert = xTaskCreate(task_alert, "ALERT_TASK", 1024, NULL, 0, &handle_task_alert);

    if (status_task_led == pdPASS && status_task_sensor == pdPASS && status_task_alert == pdPASS) {
        // Start the scheduler
        vTaskStartScheduler();
    } else {
        // We should never get here as control is now taken by the scheduler
        server_error("Insufficient RAM to start default tasks");
    }

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
 *        USER LED, and PB11, which is triggers an interrupt when it
 *        goes high (in response to alert signal from MCP9808).
 */
static void init_gpio(void) {

    // Enable the clock for GPIO ports A (USER LED), F (MCP9808 INT PIN)
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    // Clear the LED
    HAL_GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_PIN_RESET);

    // Configure GPIO pin for the Nucleo's USER LED
    GPIO_InitTypeDef led_init_data = {0};
    led_init_data.Pin   = LED_GPIO_PIN;
    led_init_data.Mode  = GPIO_MODE_OUTPUT_PP;
    led_init_data.Pull  = GPIO_PULLUP;
    led_init_data.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(LED_GPIO_PORT, &led_init_data);

    // Configure GPIO pin for the MCP9808 interrupt (PD8)
    GPIO_InitTypeDef mcp_init_data = {0};
    mcp_init_data.Pin   = MCP_INT_PIN;
    mcp_init_data.Mode  = GPIO_MODE_IT_FALLING;
    mcp_init_data.Pull  = GPIO_NOPULL;
    HAL_GPIO_Init(MCP_GPIO_PORT, &mcp_init_data);

    // Set up the NVIC to process interrupts
    // IMPORTANT FOR CORTEX-M on STM32 Use no sub-prority bits...
    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

    // ...and make sure the priority is numerically just lower than 
    // configMAX_SYSCALL_INTERRUPT_PRIORITY (but not 0-3)
    HAL_NVIC_SetPriority(MCP_INT_IRQ, configMAX_SYSCALL_INTERRUPT_PRIORITY -1, 0);
    HAL_NVIC_EnableIRQ(MCP_INT_IRQ);

    // FOR MORE INFORMATION, PLEASE SEE:
    // https://www.freertos.org/RTOS-Cortex-M3-M4.html
}


/**
 * @brief  Function implementing the LED flasher task.
 *         Blinks the USER LED if there is no alert in progress.
 *
 * @param  argument: Not used
 */
static void task_led(void *argument) {

    // Get the pause period in ticks from a millisecond value
    const TickType_t led_pause_ticks = pdMS_TO_TICKS(LED_FLASH_INTERVAL_MS);

    while(1) {
        // Toggle the NDB's USER LED
        if (!alert_fired) HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_GPIO_PIN);

        // Yield execution for a period
        vTaskDelay(led_pause_ticks);
    }
}


/**
 * @brief  Function implementing the MCP9808 temperature read task.
 *         Gets and logs the current temperature.
 *
 * @param  argument: Not used
 */
static void task_sensor(void *argument) {

    // Get the pause period in ticks from a millisecond value
    const TickType_t ping_pause_ticks = pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS);

    while(1) {
        // Output the current reading
        if (got_mcp9808) {
            current_temp = MCP9808_read_temp();
        }

        GPIO_PinState state = HAL_GPIO_ReadPin(MCP_GPIO_PORT, MCP_INT_PIN);
        bool asserted = MCP9808_get_alert_state();
        server_log("Current temperature: %.2f°C (PD8 %s, ALRT %s)", current_temp, (state == GPIO_PIN_SET ? "SET" : "CLEAR"), (asserted ? "SET" : "CLEAR"));

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

    while (1) {
        // Block until a notification arrives
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // Show the IRQ was hit
        HAL_GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_PIN_SET);

        // Set and start a timer to clear the alert
        set_alert_timer();
     }
}


/**
 * @brief Set and start a timer to clear the current alert.
 *        The function `timer_fired_callback()` is called when
 *        the timer fires.
 */
static void set_alert_timer(void) {

    // Set up and start a FreeRTOS timer
    alert_timer = xTimerCreate("ALERT_TIMER",
                               pdMS_TO_TICKS(ALERT_DISPLAY_PERIOD_MS),
                               pdFALSE,
                               (void*)0,
                               timer_fired_callback);
    if (alert_timer != NULL) xTimerStart(alert_timer, SENSOR_TASK_WAIT_TICKS);
}


/**
 * @brief Callback actioned when the post IRQ timer fires.
 *
 * @param timer: The triggering timer.
 */
static void timer_fired_callback(TimerHandle_t timer) {

    // Check whether the alert condition has passed
    // NOTE The MCP980 does not signal this on the ALERT pin
    current_temp = MCP9808_read_temp();
    if (current_temp < (double)TEMP_UPPER_LIMIT_C) {
        // Clear the LED and the alert
        HAL_GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_PIN_RESET);
        alert_fired = false;

        // Clear the timer
        alert_timer = NULL;
    } else {
        // Temperature still too high -- restart the timer
        set_alert_timer();
    }
}


/**
 * @brief Interrupt handler as specified by the STM32U5 HAL.
 */
void EXTI11_IRQHandler(void) {

    HAL_GPIO_EXTI_IRQHandler(MCP_INT_PIN);
}


/**
 * @brief IRQ handler as specified by the STM32U5 HAL.
 *
 * @param GPIO_Pin The pin the triggered the IRQ
 */
void HAL_GPIO_EXTI_Falling_Callback(uint16_t pin) {

    // Make sure the LED flasher task doesn't flash the LED
    alert_fired = true;
    
    // Signal the alert clearance task
    // IMPORTANT Calling FreeRTOS functions from ISRs requires
    //           close attention. Use `...FromISR()` versions of 
    //           calls, and ensure the IRQs which trigger this
    //           handler have a suitable priority -- see
    //           https://www.freertos.org/RTOS-Cortex-M3-M4.html
    //           and `init_gpio()a, above.
    BaseType_t higher_priority_task_woken = pdFALSE;
    vTaskNotifyGiveFromISR(handle_task_alert, &higher_priority_task_woken);
    portYIELD_FROM_ISR(higher_priority_task_woken);
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


