cmake_minimum_required(VERSION 3.14)

# Set application data
set(APP "Microvisor Native FreeRTOS Demo")
set(VERSION_NUMBER "1.0.3")
set(BUILD_NUMBER "1")

message("Building ${APP} ${VERSION_NUMBER} build ${BUILD_NUMBER}")

# Pass in version data
include_directories(${CMAKE_CURRENT_BINARY_DIR})
configure_file(app_version.in app_version.h @ONLY)

# Compile app source code file(s)
add_executable(${PROJECT_NAME}
    main.c
    i2c.c
    logging.c
    mcp9808.c
    stm32u5xx_hal_timebase_tim_template.c
)

# Link built libraries
target_link_libraries(${PROJECT_NAME} LINK_PUBLIC
    ST_Code
    Microvisor-HAL-STM32U5
    FreeRTOS
)

# Output additional artefacts and format the output binary
add_custom_command(OUTPUT EXTRA_FILES DEPENDS ${PROJECT_NAME}
    COMMAND mv "${PROJECT_NAME}" "${PROJECT_NAME}.elf"
    COMMAND ${CMAKE_SIZE} --format=berkeley "${PROJECT_NAME}.elf"
    COMMAND ${CMAKE_OBJDUMP} -h -S "${PROJECT_NAME}.elf" > "${PROJECT_NAME}.list"
    COMMAND ${CMAKE_OBJCOPY} -O binary "${PROJECT_NAME}.elf" "${PROJECT_NAME}.bin"
)

# Prepare the additional files
add_custom_target(extras ALL DEPENDS EXTRA_FILES)
