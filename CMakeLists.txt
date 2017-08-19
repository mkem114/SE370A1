cmake_minimum_required(VERSION 3.8)
project(SE370A1)

set(CMAKE_C_STANDARD 11)

set(SOURCE_FILES main.c)
set(SOURCE_FILES OSA1.c)
add_executable(SE370A1 ${SOURCE_FILES})