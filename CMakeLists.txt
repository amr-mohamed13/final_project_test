cmake_minimum_required(VERSION 3.30)
include(cmake/ArmToolchain.cmake)

project(dual-elevator C)

set(CMAKE_C_STANDARD                11)
set(CMAKE_C_STANDARD_REQUIRED       ON)
set(CMAKE_C_EXTENSIONS              ON)
enable_language(ASM)

set(PROJECT_PATH                         ${CMAKE_CURRENT_SOURCE_DIR})
include(cmake/Device.cmake)
include(cmake/SourceList.cmake)
include(cmake/IncludeList.cmake)

set(LINKER_SCRIPTS_BASE_DIR              ${PROJECT_PATH}/STM32-base/linker/)
set(STARTUP_BASE_DIR                     ${PROJECT_PATH}/STM32-base/startup/)

# ============================================================
#  Remove per-MCU main files from the shared source list
# ============================================================
list(FILTER SOURCE_LIST EXCLUDE REGEX ".*/src/master_main\\.c$")
list(FILTER SOURCE_LIST EXCLUDE REGEX ".*/src/slave_main\\.c$")
list(FILTER SOURCE_LIST EXCLUDE REGEX ".*/src/main\\.c$")

# ============================================================
#  Helper macro: define one firmware target
# ============================================================
macro(add_firmware TARGET_NAME MAIN_FILE MCU_DEFINE)
    add_executable(${TARGET_NAME}
            ${SOURCE_LIST}
            ${MAIN_FILE}
            ${STARTUP_BASE_DIR}/${SERIES_FOLDER}/${MAPPED_DEVICE}.s
    )
    target_include_directories(${TARGET_NAME} PUBLIC ${INCLUDE_LIST})
    target_compile_definitions(${TARGET_NAME} PUBLIC ${COMPILER_DEFINE} ${MCU_DEFINE})
    target_compile_options(${TARGET_NAME} PUBLIC
            ${CPU_PARAMETERS}
            -Wall
            -Wpedantic
            -Wno-unused-parameter
    )
    target_link_options(${TARGET_NAME} PRIVATE
            ${CPU_PARAMETERS}
            -L${LINKER_SCRIPTS_BASE_DIR}
            -T${LINKER_SCRIPTS_BASE_DIR}/${SERIES_FOLDER}/${DEVICE}.ld
            -Xlinker --gc-sections
            -Wl,-Map=${TARGET_NAME}.map
            --specs=nano.specs
    )
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${TARGET_NAME}>
    )
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${TARGET_NAME}> ${TARGET_NAME}.hex
    )
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${TARGET_NAME}> ${TARGET_NAME}.bin
    )
endmacro()

# ============================================================
#  Build BOTH targets in a single configuration
# ============================================================
add_firmware(elevator-master  ${PROJECT_PATH}/src/master_main.c  MASTER_MCU)
add_firmware(elevator-slave   ${PROJECT_PATH}/src/slave_main.c   SLAVE_MCU)