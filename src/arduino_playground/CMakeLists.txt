set(ARDUINO_PORT "/dev/ttyACM4" CACHE STRING "Upload port")
set(ARDUINO_FQBN "teensy:avr:teensy41" CACHE STRING "Arduino FQBN")
set(ARDUINO_MCU "TEENSY41" CACHE STRING "Teensy MCU type for teensy_loader_cli")
set(ARDUINO_BAUD "921600" CACHE STRING "Serial monitor baud rate")

# add_arduino_sketch(<sketch_dir> [extra arduino-cli args...])
#
# Creates <stem>-compile, <stem>-upload, and <stem>-monitor targets for an
# Arduino sketch whose directory is <sketch_dir>. The stem is the directory
# name, which must match the .ino filename (Arduino convention).
#
# Any arguments after <sketch_dir> are forwarded verbatim to arduino-cli
# compile (via ARGN), so individual sketches can pass extra --library or
# --build-property flags without modifying this function.
#
# DaisySP:
#   cmake/DaisySPArduinoShim.cmake symlinks DaisySP headers into src/Filters/
#   and generates src/daisysp_svf.cpp.  Because src/ is already -I'd
#   unconditionally, the compiler finds "Filters/svf.h" without any resolver
#   involvement.  DAISYSP_BUILD_FLAGS_CPP overrides build.flags.cpp to enable
#   C++20 (required for std::span used in AudioNode.h).
function(add_arduino_sketch SKETCH_DIR)
    get_filename_component(sketch_stem ${SKETCH_DIR} NAME)
    set(build_dir ${CMAKE_BINARY_DIR}/arduino/${sketch_stem})
    set(hex_file ${build_dir}/${sketch_stem}.ino.hex)

    add_custom_target(${sketch_stem}-compile
        COMMAND arduino-cli compile
            --fqbn ${ARDUINO_FQBN}
            --build-path ${build_dir}
            --library "${PROJECT_SOURCE_DIR}/src"
            --library "${PROJECT_SOURCE_DIR}/src/arduino_playground"
            --build-property "${DAISYSP_BUILD_FLAGS_CPP}"
            ${ARGN}
            ${SKETCH_DIR} --verbose
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        BYPRODUCTS ${hex_file}
        USES_TERMINAL
    )

    # Create alias - I intuitively use sketch dir/sketch name as the compile target name.
    add_custom_target(${sketch_stem})
    add_dependencies(${sketch_stem} ${sketch_stem}-compile)

    add_custom_target(${sketch_stem}-upload
        # -s: soft-reboot the board into the bootloader over its running
        # sketch's USB serial port, so upload doesn't need a physical press
        # of the reset button (needed for unattended/self-driven reflashes).
        COMMAND teensy_loader_cli --mcu=${ARDUINO_MCU} -w -s ${hex_file}
        USES_TERMINAL
    )

    add_custom_target(${sketch_stem}-monitor
        COMMAND arduino-cli monitor -p ${ARDUINO_PORT} -c baudrate=${ARDUINO_BAUD}
        USES_TERMINAL
    )
endfunction()

add_subdirectory(SeqnautDemo)
add_subdirectory(TransientDetectorDemo)
