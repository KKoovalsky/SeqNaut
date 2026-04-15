add_executable(poly_voice_host main.cpp)

target_link_libraries(poly_voice_host PRIVATE
    sycosm::core
    DaisySP
)
