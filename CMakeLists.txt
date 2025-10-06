cmake_minimum_required(VERSION 3.11)
include(FetchContent)
project(silly_packer CXX)

set(STB_IMPLEMENT_IMAGE ON CACHE BOOL "")
set(STB_IMPLEMENT_IMAGE_WRITE ON CACHE BOOL "")
set(STB_IMPLEMENT_IMAGE_RESIZE2 ON CACHE BOOL "")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/cmake/stb)

FetchContent_Declare(
    argparse
    GIT_REPOSITORY https://github.com/morrisfranken/argparse.git
    GIT_SHALLOW 1
)
FetchContent_MakeAvailable(argparse)

file(GLOB_RECURSE PROJECT_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 20)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD_REQUIRED ON)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(${PROJECT_NAME} PRIVATE stb argparse)
