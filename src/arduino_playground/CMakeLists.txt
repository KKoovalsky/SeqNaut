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
function(add_arduino_sketch SKETCH_DIR)
    get_filename_component(sketch_stem ${SKETCH_DIR} NAME)
    set(build_dir ${CMAKE_BINARY_DIR}/arduino/${sketch_stem})
    set(hex_file ${build_dir}/${sketch_stem}.ino.hex)

    # arduino-cli only resolves library include paths for headers that appear
    # directly in sketch files.  Headers inside library headers (e.g.
    # TransientDetector.h → Filters/svf.h) are never scanned, so DaisySP's
    # src/ tree must be injected explicitly via --build-property.
    set(_dsp "${PROJECT_SOURCE_DIR}/vendor/DaisySP/src")
    add_custom_target(${sketch_stem}-compile
        COMMAND arduino-cli compile
            --fqbn ${ARDUINO_FQBN}
            --build-path ${build_dir}
            --library "${PROJECT_SOURCE_DIR}/src"
            --library "${PROJECT_SOURCE_DIR}/src/arduino_playground"
            --library "${PROJECT_SOURCE_DIR}/vendor/DaisySP"
            --build-property "compiler.cpp.extra_flags=-I${_dsp} -I${_dsp}/Filters -I${_dsp}/Utility"
            ${ARGN}
            ${SKETCH_DIR} --verbose
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        BYPRODUCTS ${hex_file}
        USES_TERMINAL
    )

    add_custom_target(${sketch_stem}-upload
        COMMAND teensy_loader_cli --mcu=${ARDUINO_MCU} -w ${hex_file}
        USES_TERMINAL
    )

    add_custom_target(${sketch_stem}-monitor
        COMMAND arduino-cli monitor -p ${ARDUINO_PORT} -c baudrate=${ARDUINO_BAUD}
        USES_TERMINAL
    )
endfunction()

add_subdirectory(SeqnautDemo)
