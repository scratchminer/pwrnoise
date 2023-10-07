cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

find_package(SDL2 CONFIG REQUIRED)

set(SOURCE_DIR ${CMAKE_SOURCE_DIR}/src)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

project(pwrnoise LANGUAGES C)

add_executable(pwrnoise "")
target_sources(pwrnoise PRIVATE ${SOURCE_DIR}/pwrnoise.c ${SOURCE_DIR}/blip_buf.c)
target_include_directories(pwrnoise PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(pwrnoise ${SDL2_LIBRARIES})