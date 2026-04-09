cmake_minimum_required(VERSION 3.20)

set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type")

project(SeqnautDemo C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS ON)

add_subdirectory(src)
add_subdirectory(src/arduino_playground)
