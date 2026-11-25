cmake_minimum_required(VERSION 3.20)
project(mksm_xbox360_guest_endian_smoke LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(guest_endian_smoke endian_test.cpp)
target_include_directories(guest_endian_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

enable_testing()
add_test(NAME guest_endian_smoke COMMAND guest_endian_smoke)
