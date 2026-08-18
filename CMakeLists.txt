
# Copyright 2026 NXP
#
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.22.0)

cmake_path(APPEND conf_file_path ${CMAKE_CURRENT_LIST_DIR} ${board} prj.conf)
set(CONF_FILE ${conf_file_path} ${CONF_FILE})
include(${SdkRootDirPath}/cmake/extension/mcux.cmake)

project(freertos_project LANGUAGES C CXX ASM PROJECT_BOARD_PORT_PATH ${board_root}/${board}/freertos_examples/freertos_hello)

include(${SdkRootDirPath}/CMakeLists.txt)


include(${board}/board_files.cmake)

mcux_add_source(
    SOURCES freertos_hello.c
            app/app_rgb_task.c
            app/app_rgb_task.h
            bsp/bsp_rgb_led.c
            bsp/bsp_rgb_led.h
            bsp/bsp_spi.c
            bsp/bsp_spi.h
            bsp/bsp_oled_12864.c
            bsp/bsp_oled_12864.h
            bsp/bsp_oled_data.c
            bsp/bsp_oled_data.h
            
)

mcux_add_include(
    INCLUDES .
)
mcux_convert_binary(BINARY ${APPLICATION_BINARY_DIR}/${MCUX_SDK_PROJECT_NAME}.bin)

