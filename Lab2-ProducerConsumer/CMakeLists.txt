cmake_minimum_required(VERSION 4.3)
project(Lab2_ProducerConsumer)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Threads REQUIRED)

add_executable(Lab2_ProducerConsumer
        main.cpp
        src/BoundedQueue.h)

target_compile_options(Lab2_ProducerConsumer PRIVATE -Wall -Wextra)
target_link_libraries(Lab2_ProducerConsumer PRIVATE Threads::Threads)
