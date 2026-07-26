cmake_minimum_required(VERSION 3.22)

# Bare-metal ARM target. These must be set before project()/enable_language().
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR cortex-m4)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_SIZE arm-none-eabi-size)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_PROJECT_NAME firmware)
project(${CMAKE_PROJECT_NAME} C ASM)

set(CMAKE_EXECUTABLE_SUFFIX .elf)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C compiler: ${CMAKE_C_COMPILER}")

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

set(CPU_FLAGS
    -mcpu=cortex-m4
    -mthumb
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard
)

set(COMMON_C_FLAGS
    ${CPU_FLAGS}
    -Wall
    -Wextra
    -ffunction-sections
    -fdata-sections
)

# Apply CPU/ABI flags globally because CubeMX creates several targets
# (stm32cubemx, ThreadX, USBX, etc.). If these flags are only attached to the
# final firmware target, the ThreadX assembly may be assembled as a baseline
# Thumb target and fail on Cortex-M instructions such as MRS/MSR PRIMASK.
string(JOIN " " CPU_FLAGS_STR ${CPU_FLAGS})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CPU_FLAGS_STR} -Wall -Wextra -ffunction-sections -fdata-sections")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} ${CPU_FLAGS_STR} -x assembler-with-cpp")

set(CMAKE_C_FLAGS_DEBUG "-Og -g3")
set(CMAKE_C_FLAGS_RELEASE "-O2 -g0")

# CubeMX usually places this in the project root. Adjust if your file name differs.
set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/STM32F407XX_FLASH.ld)

add_executable(${CMAKE_PROJECT_NAME})

# CubeMX-generated library target, including HAL/CMSIS and, if enabled,
# ThreadX/USBX/Middleware sources.
add_subdirectory(cmake/stm32cubemx)

target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    USE_HAL_DRIVER
    STM32F407xx
)

target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    ${CPU_FLAGS}
    -T${LINKER_SCRIPT}
    -Wl,-Map=${CMAKE_PROJECT_NAME}.map
    -Wl,--gc-sections
    -Wl,--print-memory-usage
    -specs=nano.specs
    -specs=nosys.specs
)

# User sources can be added here if they are not already included by CubeMX.
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    AZURE_RTOS/App/thread_toggle.c
    AZURE_RTOS/App/thread_memtest.c
    AZURE_RTOS/App/logger.c
)

# User include directories can be added here if needed.
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
)

# Keep CubeMX-generated code in the separate stm32cubemx target.
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
)

# Convenience outputs for DFU/J-Link flashing.
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${CMAKE_PROJECT_NAME}.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${CMAKE_PROJECT_NAME}.hex
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
    COMMENT "Generating ${CMAKE_PROJECT_NAME}.bin/.hex and printing size"
)
