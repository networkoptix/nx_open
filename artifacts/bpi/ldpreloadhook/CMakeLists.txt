cmake_minimum_required(VERSION 3.3)
project(proxy_decoder)

# ATTENTION: This cmake file is not intended for building but rather just for IDE convenience.

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -DCLOCK_REALTIME='(int)0'")

set(SOURCE_FILES
    hook_common.h
    hook.c
    dev_fb.h
    dev_fb.c
    handle_fb_disp.h
    ldpreloadhookhandle_fb_disp.c
)

# Directory containing all necessary headers from the libraries used.
include_directories(
    ../../../bpi/src/include
)

add_executable(ldpreloadhook.so ${SOURCE_FILES})
