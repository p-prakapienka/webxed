cmake_minimum_required(VERSION 3.15)
project(dn2pst LANGUAGES CXX
        DESCRIPTION "Reader for Elektron Digitone II .dn2pst sound files")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ZLIB REQUIRED)

add_executable(dn2pst main.cpp dn2pst.hpp)
target_link_libraries(dn2pst PRIVATE ZLIB::ZLIB)
target_include_directories(dn2pst PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
  target_compile_options(dn2pst PRIVATE /W4)
else()
  target_compile_options(dn2pst PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Regression check: the parameter table must match baseline.txt exactly.
enable_testing()
add_test(NAME baseline
         COMMAND ${CMAKE_COMMAND}
                 -DEXE=$<TARGET_FILE:dn2pst>
                 -DDIR=${CMAKE_CURRENT_SOURCE_DIR}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_baseline.cmake)
