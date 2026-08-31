cmake_minimum_required(VERSION 4.3)
project(EDA)

set(CMAKE_CXX_STANDARD 23)

include_directories(src)

add_executable(EDA
        src/main.cpp
        src/main.h)
