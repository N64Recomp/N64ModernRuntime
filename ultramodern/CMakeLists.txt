project(ultramodern)
cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

if(UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif()

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
    "${CMAKE_CURRENT_SOURCE_DIR}/src/timer.cpp"
)

target_include_directories(ultramodern PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include/"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/ultramodern" # FIXME: remove redundant path
    "${CMAKE_SOURCE_DIR}/thirdparty/concurrentqueue"
)

# TODO: remove when rt64 is no longer a hard dependency
target_include_directories(ultramodern PRIVATE
    "${CMAKE_SOURCE_DIR}/rt64/src"
    "${CMAKE_SOURCE_DIR}/rt64/src/contrib"
    "${CMAKE_SOURCE_DIR}/rt64/src/contrib/hlslpp/include"
    "${CMAKE_SOURCE_DIR}/rt64/src/contrib/dxc/inc"
    "${CMAKE_SOURCE_DIR}/rt64/src/rhi"
    "${CMAKE_SOURCE_DIR}/rt64/src/render"
    "${CMAKE_SOURCE_DIR}/rt64/src/contrib/nativefiledialog-extended/src/include"
)

# TODO: remove when librecomp is untangled from ultramodern
target_include_directories(ultramodern PRIVATE
    "${CMAKE_SOURCE_DIR}/librecomp/include"
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

    # Copy SDL2 and dxc DLLs to output folder as post build step
    add_custom_command(TARGET ultramodern POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${sdl2_SOURCE_DIR}/lib/x64/SDL2.dll"
            "${CMAKE_SOURCE_DIR}/lib/rt64/src/contrib/dxc/bin/x64/dxil.dll"
            "${CMAKE_SOURCE_DIR}/lib/rt64/src/contrib/dxc/bin/x64/dxcompiler.dll"
            $<TARGET_FILE_DIR:ultramodern>)
endif()

if (LINUX)
    find_package(SDL2 REQUIRED)
    find_package(X11 REQUIRED)

    message(STATUS "SDL2_FOUND = ${SDL2_FOUND}")
    message(STATUS "SDL2_INCLUDE_DIRS = ${SDL2_INCLUDE_DIRS}")

    target_include_directories(ultramodern PRIVATE ${SDL2_INCLUDE_DIRS})

    message(STATUS "X11_FOUND = ${X11_FOUND}")
    message(STATUS "X11_Xrandr_FOUND = ${X11_Xrandr_FOUND}")
    message(STATUS "X11_INCLUDE_DIR = ${X11_INCLUDE_DIR}")
    message(STATUS "X11_LIBRARIES = ${X11_LIBRARIES}")

    target_include_directories(ultramodern PRIVATE ${X11_INCLUDE_DIR} ${X11_Xrandr_INCLUDE_PATH})
    target_link_libraries(ultramodern PRIVATE ${X11_LIBRARIES} ${X11_Xrandr_LIB})
endif()
