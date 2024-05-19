cmake_minimum_required(VERSION 3.20)
project(N64ModernRuntime)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_subdirectory(librecomp)
add_subdirectory(ultramodern)

set(RT64_STATIC TRUE)
add_subdirectory(rt64)
