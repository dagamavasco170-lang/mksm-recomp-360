cmake_minimum_required(VERSION 3.25)

project(MKSMXbox360Smoke LANGUAGES CXX)

if(NOT XSDK)
    message(FATAL_ERROR
        "Configure with the xSDK toolchain: -DCMAKE_TOOLCHAIN_FILE=<xsdk>/cmake/xsdk.cmake")
endif()

option(MKSM_XENIA_HEADLESS_SMOKE
    "Build a finite diagnostics-only XEX for Xenia CI (no Xenos presentation)" OFF)

add_executable(mksm_xbox360_smoke
    smoke.cpp
    ps2_pad_xbox360.cpp
    ps2_pad_mapping.cpp
)

target_include_directories(mksm_xbox360_smoke PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../ps2xRuntime/include"
)

set(MKSM_XBOX360_GENERATED_DIR "" CACHE PATH
    "Host-generated PS2Recomp output directory for the scalar Xenon ABI probe")
if(MKSM_XBOX360_GENERATED_DIR)
    foreach(required_file IN ITEMS
            "ps2_recompiled_functions.cpp"
            "ps2_recompiled_functions.h")
        if(NOT EXISTS "${MKSM_XBOX360_GENERATED_DIR}/${required_file}")
            message(FATAL_ERROR
                "Missing host-generated file: ${MKSM_XBOX360_GENERATED_DIR}/${required_file}")
        endif()
    endforeach()

    target_sources(mksm_xbox360_smoke PRIVATE
        "${MKSM_XBOX360_GENERATED_DIR}/ps2_recompiled_functions.cpp"
        "ps2_recomp_generated_xenon_probe.cpp"
    )
    # Generated functions have the common runtime ABI parameters even when a
    # focused scalar probe does not touch RDRAM or the runtime object.
    set_source_files_properties(
        "${MKSM_XBOX360_GENERATED_DIR}/ps2_recompiled_functions.cpp"
        PROPERTIES COMPILE_OPTIONS "-Wno-unused-parameter"
    )
    target_include_directories(mksm_xbox360_smoke PRIVATE
        "${MKSM_XBOX360_GENERATED_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
    )
    target_compile_definitions(mksm_xbox360_smoke PRIVATE
        PS2X_GENERATED_RUNTIME_ABI=1
        MKSM_HAS_GENERATED_RECOMP_PROBE=1
    )
endif()
if(MKSM_XENIA_HEADLESS_SMOKE)
    target_compile_definitions(mksm_xbox360_smoke PRIVATE MKSM_XENIA_HEADLESS_SMOKE=1)
endif()

target_compile_features(mksm_xbox360_smoke PRIVATE cxx_std_20)
target_compile_options(mksm_xbox360_smoke PRIVATE -Wall -Wextra -Werror)

xsdk_add_xex(mksm_xbox360_smoke
    TITLE_ID 0x4D4B534D
    NAME "MK Shaolin Monks X360 Smoke"
    VERSION 1.0.0.0
)
