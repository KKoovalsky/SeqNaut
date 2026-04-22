add_executable(transient_detector main.cpp)

target_link_libraries(transient_detector PRIVATE
    sycosm::core
    DaisySP
    host::dr_libs
)
