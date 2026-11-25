cmake_minimum_required(VERSION 3.25)
project(mksm_xbox360_smoke C)

add_executable(mksm_xbox360_smoke main.c)
set_target_properties(mksm_xbox360_smoke PROPERTIES C_STANDARD 17 C_EXTENSIONS ON)
target_compile_options(mksm_xbox360_smoke PRIVATE -Wall -Wextra -Werror)
xsdk_add_xex(mksm_xbox360_smoke)
