cmake_minimum_required(VERSION 3.1)

## Use the variable PROJECT_NAME for changing the target name
set( PROJECT_NAME "partclone-nbd" )
set( CMAKE_BUILD_TYPE Debug )
set( CMAKE_C_STANDARD 99 )

## Set our project name
project(${PROJECT_NAME})

## Use all the *.cpp files we found under this folder for the project
FILE(GLOB SRCS "src/*.c")

## Specify headers path
include_directories(include)

## Define the executable
add_executable(${PROJECT_NAME} ${SRCS})

## find pthread library
find_package( Threads )

target_link_libraries(
    ${PROJECT_NAME}  ${CMAKE_THREAD_LIBS_INIT}
)

## install rules
install (TARGETS partclone-nbd DESTINATION bin)
