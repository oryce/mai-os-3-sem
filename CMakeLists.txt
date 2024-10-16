cmake_minimum_required(VERSION 3.27)

project(mai_os_3_sem)

set(CMAKE_C_STANDARD "99")
set(CMAKE_C_FLAGS "-Wall -pedantic")

include(cmake/AddTask.cmake)

add_subdirectory("src/lab-1" "src/lab-1")