cmake_minimum_required(VERSION 3.20)
project(librecomp)

# Define the library
add_library(librecomp STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ai.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cont.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dp.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/eep.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/euc-jp.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/files.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/flash.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/math_routines.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/overlays.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/pak.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/patch_loading.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/pi.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/print.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/recomp.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/rsp.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/sp.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ultra_stubs.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ultra_translation.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/vi.cpp")

target_include_directories(librecomp PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/../ultramodern/include"
    "${PROJECT_SOURCE_DIR}/../thirdparty/concurrentqueue"
)
target_include_directories(librecomp PRIVATE
    "${PROJECT_SOURCE_DIR}/../rt64/src/contrib"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/librecomp"
)

target_compile_options(librecomp PRIVATE -Wno-deprecated-declarations)

target_link_libraries(librecomp PRIVATE ultramodern)
