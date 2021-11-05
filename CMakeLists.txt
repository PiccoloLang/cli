cmake_minimum_required(VERSION 3.17)
project(piccolo C)

set(CMAKE_C_STANDARD 99)

file(GLOB_RECURSE APP_SOURCES piccolo/*.c piccolo/*.h)
add_executable(piccolo main.c ${APP_SOURCES})

if(${CMAKE_SYSTEM_NAME} EQUAL "Linux")
    target_link_libraries(${PROJECT_NAME} m)
endif()