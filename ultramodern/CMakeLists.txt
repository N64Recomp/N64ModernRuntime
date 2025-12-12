cmake_minimum_required(VERSION 3.20)

project(ultramodern)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(ultramodern STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/audio.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/error_handling.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/events.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/extensions.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/input.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/mesgqueue.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/misc_ultra.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/renderer_context.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/rsp.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/scheduling.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/task_win32.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/threadqueue.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/threads.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/timer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ultrainit.cpp"
)

target_include_directories(ultramodern PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include/"
    "${PROJECT_SOURCE_DIR}/../thirdparty"
    "${PROJECT_SOURCE_DIR}/../thirdparty/concurrentqueue"
    "${PROJECT_SOURCE_DIR}/../thirdparty/sse2neon"
)

target_compile_options(ultramodern PRIVATE
#    -Wall
#    -Wextra
    -Wno-unused-parameter
)

if (WIN32)
    add_compile_definitions(NOMINMAX)
endif()
