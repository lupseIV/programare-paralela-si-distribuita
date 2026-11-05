cmake_minimum_required(VERSION 4.0)
project(Lab3_SimpleParallelTasks)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Threads REQUIRED)

add_executable(Lab3_SimpleParallelTasks
        main.cpp
        src/Matrix.h)

target_compile_options(Lab3_SimpleParallelTasks PRIVATE -Wall -Wextra)
target_link_libraries(Lab3_SimpleParallelTasks PRIVATE Threads::Threads)
