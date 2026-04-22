find_package(PkgConfig REQUIRED)
pkg_check_modules(portaudio REQUIRED portaudio-2.0)

if(NOT TARGET host::portaudio)
    add_library(host::portaudio INTERFACE IMPORTED GLOBAL)
    target_include_directories(host::portaudio INTERFACE ${portaudio_INCLUDE_DIRS})
    target_link_libraries(host::portaudio INTERFACE ${portaudio_LIBRARIES})
endif()
