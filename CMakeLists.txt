cmake_minimum_required(VERSION 3.27)

project(mai_os_3_sem)

set(CMAKE_C_STANDARD "99")
set(CMAKE_C_FLAGS "-Wall -pedantic")

set(CMAKE_CXX_STANDARD "20")
set(CMAKE_CXX_FLAGS "-Wall -pedantic")

if (CMAKE_BUILD_TYPE STREQUAL "TSAN")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -fsanitize=thread")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fsanitize=thread")
endif ()

include(cmake/AddTask.cmake)

add_subdirectory("src/lab-1")
add_subdirectory("src/lab-2")