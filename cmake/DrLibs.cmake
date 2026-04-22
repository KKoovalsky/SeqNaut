include(FetchContent)

FetchContent_Declare(
    dr_libs
    GIT_REPOSITORY https://github.com/mackron/dr_libs.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(dr_libs)

if(NOT TARGET host::dr_libs)
    add_library(host::dr_libs INTERFACE IMPORTED GLOBAL)
    target_include_directories(host::dr_libs INTERFACE ${dr_libs_SOURCE_DIR})
endif()
