
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

# 收集u8g2核心C源码
# CONFIGURE_DEPENDS表示当u8g2/csrc目录中的源文件发生变化时，CMake会重新检查文件列表。
file(GLOB U8G2_SOURCES CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/u8g2/csrc/*.c"
)

mcux_add_source(
    SOURCES freertos_hello.c
            app/app_rgb_task.c
            app/app_rgb_task.h
            bsp/bsp_rgb_led.c
            bsp/bsp_rgb_led.h
            bsp/bsp_spi.c
            bsp/bsp_spi.h
            bsp/bsp_button.c
            bsp/bsp_button.h
            bsp/bsp_buzzer.c
            bsp/bsp_buzzer.h
            bsp/bsp_i3c.c
            bsp/bsp_i3c.h
            bsp/bsp_p3t1755.c
            bsp/bsp_p3t1755.h
            UI/menu.c
            UI/menu.h
            UI/ui_settings.c
            UI/ui_settings.h
            UI/ui_display_port.c
            UI/ui_display_port.h
            UI/ui_animation.c
            UI/ui_animation.h
            app/app_ui_task.c
            app/app_ui_task.h
            ${U8G2_SOURCES}
            bsp/bsp_oled_hw.c
            bsp/bsp_oled_hw.h
            bsp/bsp_u8g2_port.c
            bsp/bsp_u8g2_port.h
            UI/ui_assets.c
            UI/ui_assets.h  
            UI/menu_internal.h
            UI/ui_window.c
            UI/ui_window.h
            UI/ui_effects.c
            UI/ui_effects.h
            UI/ui_waveform.c
            UI/ui_waveform.h
            UI/ui_sleep.c
            UI/ui_sleep.h
            UI/ui_platform.c
            UI/ui_platform.h
            bsp/bsp_2oleds.c
            bsp/bsp_2oleds.h
            app/app_temp_task.c
            app/app_temp_task.h       
            bsp/bsp_i2c.c
            bsp/bsp_i2c.h
            FOC/AS5600.c
            FOC/AS5600.h
            bsp/bsp_motor_pwm.c
            bsp/bsp_motor_pwm.h
            FOC/FOC.h
            FOC/FOC.c
            FOC/foc_openloop.c
            app/app_motor_task.c
            app/app_motor_task.h  
            app/app_buzzer_task.c
            app/app_buzzer_task.h     
)

mcux_add_include(
    INCLUDES 
    .
    u8g2/csrc 
)
mcux_convert_binary(
    BINARY 
    ${APPLICATION_BINARY_DIR}/${MCUX_SDK_PROJECT_NAME}.bin
)
