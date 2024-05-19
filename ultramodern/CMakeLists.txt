cmake_minimum_required(VERSION 3.20)
project(ultramodern)

add_library(ultramodern STATIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src/audio.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/events.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/mesgqueue.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/misc_ultra.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/rt64_layer.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/scheduling.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/task_win32.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/threadqueue.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/threads.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/timer.cpp")

target_include_directories(ultramodern PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_SOURCE_DIR}/thirdparty/concurrentqueue")
target_link_libraries(ultramodern PRIVATE rt64)
