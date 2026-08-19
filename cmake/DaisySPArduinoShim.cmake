# cmake/DaisySPArduinoShim.cmake
#
# Makes DaisySP available to Arduino (arduino-cli) builds.
# Include this after FetchContent_MakeAvailable(DaisySP).
#
# Problem summary:
#   arduino-cli's library resolver matches includes by filename only.
#   A path-qualified include like "Filters/svf.h" (used in TransientDetector.h)
#   is never matched to any library, so no --library path helps.
#
# Solution:
#   Symlink the needed DaisySP files into src/Filters/ — the src/ library
#   (sycosm-core) already has its root unconditionally in every compilation's
#   -I list, so the compiler finds "Filters/svf.h" as src/Filters/svf.h without
#   any resolver involvement.
#
#   A unity file src/daisysp_svf.cpp (at the sycosm-core root) compiles
#   svf.cpp.  GCC's current-file-directory search resolves svf.cpp's internal
#   includes (svf.h, dsp.h) relative to src/Filters/ automatically.
#
# C++20:
#   std::span (used in AudioNode.h) requires C++20.  Teensy's boards.txt sets
#   build.flags.cpp to -std=gnu++17.  We expose DAISYSP_BUILD_FLAGS_CPP so
#   add_arduino_sketch can override that property.

FetchContent_GetProperties(DaisySP SOURCE_DIR _daisysp_src)

if(NOT _daisysp_src)
    message(FATAL_ERROR
        "DaisySPArduinoShim: DaisySP source directory is empty — "
        "call FetchContent_MakeAvailable(DaisySP) before including this module.")
endif()

set(_src "${PROJECT_SOURCE_DIR}/src")
set(_dsp_filters "${_daisysp_src}/Source/Filters")
set(_dsp_utility "${_daisysp_src}/Source/Utility")

file(MAKE_DIRECTORY "${_src}/Filters")

foreach(_file svf.h svf.cpp)
    if(NOT IS_SYMLINK "${_src}/Filters/${_file}")
        file(CREATE_LINK "${_dsp_filters}/${_file}" "${_src}/Filters/${_file}" SYMBOLIC)
    endif()
endforeach()

if(NOT IS_SYMLINK "${_src}/Filters/dsp.h")
    file(CREATE_LINK "${_dsp_utility}/dsp.h" "${_src}/Filters/dsp.h" SYMBOLIC)
endif()

file(WRITE "${_src}/daisysp_svf.cpp" "#include \"Filters/svf.cpp\"\n")

# build.flags.cpp value that adds C++20 (keeping all other Teensy 4.1 flags).
set(DAISYSP_BUILD_FLAGS_CPP
    "build.flags.cpp=-std=gnu++20 -fno-exceptions -fpermissive -fno-rtti -fno-threadsafe-statics -felide-constructors -Wno-error=narrowing -Wno-psabi"
    CACHE INTERNAL "arduino-cli --build-property to enable C++20 on Teensy")
