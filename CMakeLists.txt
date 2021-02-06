cmake_minimum_required(VERSION 3.1)

## Use the variable PROJECT_NAME for changing the target name
set( PROJECT_NAME "partclone-nbd" )
set( CMAKE_BUILD_TYPE Release )
set( CMAKE_C_STANDARD 99 )

## Set our project name
project(${PROJECT_NAME})

## Use all the *.cpp files we found under this folder for the project
FILE(GLOB SRCS "src/*.c")

## Specify headers path
include_directories(include)

## Define the executable
add_executable(${PROJECT_NAME} ${SRCS})

## We will need pthread soon ...
target_link_libraries(
    ${PROJECT_NAME} pthread
)

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Werror)

## install rules
install (TARGETS partclone-nbd DESTINATION bin)
