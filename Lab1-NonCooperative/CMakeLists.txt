cmake_minimum_required(VERSION 4.3)
project(Lab1_NonCooperative)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Threads REQUIRED)

add_executable(Lab1_NonCooperative
        main.cpp
        src/Config.cpp
        src/DataGenerator.cpp
        src/Orchestrator.cpp
        bonus/BenchmarkBonus.cpp)

target_compile_options(Lab1_NonCooperative PRIVATE -Wall -Wextra)
target_link_libraries(Lab1_NonCooperative PRIVATE Threads::Threads)
