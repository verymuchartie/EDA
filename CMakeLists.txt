cmake_minimum_required(VERSION 4.3)
project(EDA)

set(CMAKE_CXX_STANDARD 23)

include_directories(src)

add_executable(EDA
        src/sim/Core.cpp
        src/sim/Core.h
        src/main.cpp
        src/main.h
        src/sim/Primitive_Modules.h
        src/sim/Primitive_Modules.cpp
        src/sim/Dynamics.cpp
        src/sim/Dynamics.h
        src/sim/Composite_Module.cpp
        src/sim/Composite_Module.h)
