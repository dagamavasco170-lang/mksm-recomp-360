cmake_minimum_required(VERSION 3.25)

project(MKSMXbox360Smoke LANGUAGES CXX)

if(NOT XSDK)
    message(FATAL_ERROR
        "Configure with the xSDK toolchain: -DCMAKE_TOOLCHAIN_FILE=<xsdk>/cmake/xsdk.cmake")
endif()

add_executable(mksm_xbox360_smoke
    smoke.cpp
    ps2_pad_xbox360.cpp
)
target_include_directories(mksm_xbox360_smoke PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../ps2xRuntime/include"
)
target_compile_features(mksm_xbox360_smoke PRIVATE cxx_std_20)
target_compile_options(mksm_xbox360_smoke PRIVATE -Wall -Wextra -Werror)

xsdk_add_xex(mksm_xbox360_smoke
    TITLE_ID 0x4D4B534D
    NAME "MK Shaolin Monks X360 Smoke"
    VERSION 1.0.0.0
)
