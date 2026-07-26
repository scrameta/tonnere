cmake_minimum_required(VERSION 3.14)   # FetchContent_MakeAvailable needs 3.14
project(tonnerexl_host C ASM)

#
# HOST test build for the TonnereXL port (Linux).
#
# Self-contained: pulls the upstream Eclipse ThreadX + FileX Linux ports via
# FetchContent (pinned to the commits validated during development), builds the
# portable port code against them with the FAKE fpga backend, and runs the unit
# tests. This build NEVER touches your STM32 project or its ST 6.1.x RTOS.
#
# Usage:
#   cmake -B build -S .
#   cmake --build build -j
#   ./build/host_tests           # or: ctest --test-dir build --output-on-failure
#
# The port sources live one level up (../Platform, ../Filesystem, ../App); this
# CMake reaches into them so the same files feed both the host and STM32 builds.
#

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Werror)
option(TONNEREXL_SANITIZE "Enable ASan/UBSan on host build" ON)
if(TONNEREXL_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
  add_link_options(-fsanitize=address,undefined)
endif()

include(FetchContent)

# Pinned to the commits validated during development (upstream reports 6.5.1).
# To move forward deliberately, bump these hashes.
FetchContent_Declare(threadx
  GIT_REPOSITORY https://github.com/eclipse-threadx/threadx.git
  GIT_TAG        44d7c95c582d      # 6.5.1-dev, has ports/linux/gnu
)
FetchContent_Declare(filex
  GIT_REPOSITORY https://github.com/eclipse-threadx/filex.git
  GIT_TAG        9da40d41d7c3      # 6.5.1-dev, has ports/linux/gnu + fx_ram_driver
)

# ThreadX first (provides azrtos::threadx).
set(THREADX_ARCH linux CACHE STRING "" FORCE)
set(THREADX_TOOLCHAIN gnu CACHE STRING "" FORCE)
FetchContent_MakeAvailable(threadx filex)

# Third-party: don't apply -Werror to upstream code.
foreach(tgt threadx filex)
  if(TARGET ${tgt})
    target_compile_options(${tgt} PRIVATE -w)
  endif()
endforeach()

set(PORT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

add_library(tonnerexl_port STATIC
  ${PORT_ROOT}/Platform/fpga_bus_fake.c
  ${PORT_ROOT}/Filesystem/simplefile_filex.c
  ${PORT_ROOT}/App/app_threads.c
  ${PORT_ROOT}/App/app_service_steps.c
)
target_include_directories(tonnerexl_port PUBLIC
  ${PORT_ROOT}/Platform ${PORT_ROOT}/Filesystem ${PORT_ROOT}/App
)
target_compile_definitions(tonnerexl_port PUBLIC FPGA_BUS_FAKE)
target_link_libraries(tonnerexl_port PUBLIC azrtos::threadx azrtos::filex)

add_executable(host_tests
  ${PORT_ROOT}/Tests/test_main.c
  ${PORT_ROOT}/Tests/test_fpga_bus.c
  ${PORT_ROOT}/Tests/test_simplefile.c
)
target_include_directories(host_tests PRIVATE
  ${PORT_ROOT}/Tests ${PORT_ROOT}/Platform ${PORT_ROOT}/Filesystem ${PORT_ROOT}/App
)
target_link_libraries(host_tests PRIVATE tonnerexl_port azrtos::threadx azrtos::filex pthread)

enable_testing()
add_test(NAME host_tests COMMAND host_tests)
