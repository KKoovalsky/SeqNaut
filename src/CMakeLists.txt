add_library(sycosm_core INTERFACE)
add_library(sycosm::core ALIAS sycosm_core)

target_include_directories(sycosm_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
