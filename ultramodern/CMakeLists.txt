cmake_minimum_required(VERSION 3.20)

project(ultramodern)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

# Warnings
# TODO: uncoment this when we fix the warnings
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

add_library(ultramodern STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/audio.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/error_handling.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/events.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/mesgqueue.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/misc_ultra.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/rsp.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/rt64_layer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/scheduling.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/task_win32.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/threadqueue.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/threads.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/timer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ultrainit.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/user_callbacks.cpp"
)

target_include_directories(ultramodern PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include/"
    "${PROJECT_SOURCE_DIR}/../thirdparty/concurrentqueue"
    "${PROJECT_SOURCE_DIR}/../thirdparty/sse2neon"
)

target_include_directories(ultramodern PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include/ultramodern"
)

# TODO: remove when rt64 is no longer a hard dependency
target_include_directories(ultramodern PRIVATE
    "${PROJECT_SOURCE_DIR}/../rt64/src"
    "${PROJECT_SOURCE_DIR}/../rt64/src/contrib"
    "${PROJECT_SOURCE_DIR}/../rt64/src/contrib/hlslpp/include"
    "${PROJECT_SOURCE_DIR}/../rt64/src/contrib/dxc/inc"
    "${PROJECT_SOURCE_DIR}/../rt64/src/rhi"
    "${PROJECT_SOURCE_DIR}/../rt64/src/render"
    "${PROJECT_SOURCE_DIR}/../rt64/src/contrib/nativefiledialog-extended/src/include"
)

if (WIN32)
    include(FetchContent)
    # Fetch SDL2 on windows
    FetchContent_Declare(
        sdl2
        URL https://github.com/libsdl-org/SDL/releases/download/release-2.28.5/SDL2-devel-2.28.5-VC.zip
        URL_HASH MD5=d8173db078e54040c666f411c5a6afff
    )
    FetchContent_MakeAvailable(sdl2)
    target_include_directories(ultramodern PRIVATE
        ${sdl2_SOURCE_DIR}/include
    )
    target_link_directories(ultramodern PRIVATE
        ${sdl2_SOURCE_DIR}/lib/x64
    )
elseif (APPLE)
    find_package(SDL2 REQUIRED)
    target_include_directories(ultramodern PRIVATE ${SDL2_INCLUDE_DIRS})
else()
    find_package(SDL2 REQUIRED)
    find_package(X11 REQUIRED)

    message(STATUS "SDL2_FOUND = ${SDL2_FOUND}")
    message(STATUS "SDL2_INCLUDE_DIRS = ${SDL2_INCLUDE_DIRS}")

    target_include_directories(ultramodern PRIVATE ${SDL2_INCLUDE_DIRS})

    message(STATUS "X11_FOUND = ${X11_FOUND}")
    message(STATUS "X11_INCLUDE_DIR = ${X11_INCLUDE_DIR}")
    message(STATUS "X11_LIBRARIES = ${X11_LIBRARIES}")

    target_include_directories(ultramodern PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(ultramodern PRIVATE ${X11_LIBRARIES})
endif()
