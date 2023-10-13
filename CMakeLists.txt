cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

find_package(SDL2 CONFIG REQUIRED)

set(SOURCE_DIR ${CMAKE_SOURCE_DIR}/src)
set(EXTERNAL_DIR ${CMAKE_SOURCE_DIR}/external)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

project(pwrnoise LANGUAGES C)

add_executable(pwrnoise "")
target_sources(pwrnoise PRIVATE ${SOURCE_DIR}/pwrnoise.c ${EXTERNAL_DIR}/blip_buf.c ${EXTERNAL_DIR}/tinywav.c)
target_include_directories(pwrnoise PRIVATE ${SDL2_INCLUDE_DIRS} ${EXTERNAL_DIR})
target_link_libraries(pwrnoise ${SDL2_LIBRARIES})