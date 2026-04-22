add_executable(transient_detector main.cpp)

target_link_libraries(transient_detector PRIVATE
    sycosm::core
    DaisySP
    host::dr_libs
    host::portaudio
)

add_executable(live_scope live_scope.cpp)

target_link_libraries(live_scope PRIVATE
    sycosm::core
    host::portaudio
)

add_executable(live_transient live_transient.cpp)

target_link_libraries(live_transient PRIVATE
    sycosm::core
    DaisySP
    host::dr_libs
    host::portaudio
)
