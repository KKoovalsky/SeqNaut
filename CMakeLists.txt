cmake_minimum_required(VERSION 3.20)

set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type")

project(SeqnautDemo C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# Keep downloaded sources in .deps — survives `cmake --build --clean-first` and
# manual `rm -rf build/` without triggering a re-clone.
set(FETCHCONTENT_BASE_DIR "${PROJECT_SOURCE_DIR}/.deps" CACHE PATH "FetchContent dependency cache" )

include(FetchContent)

FetchContent_Declare(
    DaisySP
    GIT_REPOSITORY https://github.com/electro-smith/DaisySP.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(DaisySP)

add_subdirectory(src)
add_subdirectory(src/arduino_playground)
add_subdirectory(host)
