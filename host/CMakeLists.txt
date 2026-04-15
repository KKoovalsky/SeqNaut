include(FetchContent)

FetchContent_Declare(
    dr_libs
    GIT_REPOSITORY https://github.com/mackron/dr_libs.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(dr_libs)

add_executable(transient_detector main.cpp)

target_include_directories(transient_detector PRIVATE
    ${dr_libs_SOURCE_DIR}
)

target_link_libraries(transient_detector PRIVATE
    sycosm::core
    DaisySP
)
