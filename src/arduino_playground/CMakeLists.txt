set(ARDUINO_PORT "/dev/ttyACM4" CACHE STRING "Upload port")
set(ARDUINO_FQBN "teensy:avr:teensy41" CACHE STRING "Arduino FQBN")
set(ARDUINO_MCU "TEENSY41" CACHE STRING "Teensy MCU type for teensy_loader_cli")
set(ARDUINO_BAUD "921600" CACHE STRING "Serial monitor baud rate")

# add_arduino_sketch(<sketch_dir>)
#
# Creates <stem>-compile, <stem>-upload, and <stem>-monitor targets for an
# Arduino sketch whose directory is <sketch_dir>. The stem is the directory
# name, which must match the .ino filename (Arduino convention).
function(add_arduino_sketch SKETCH_DIR)
    get_filename_component(sketch_stem ${SKETCH_DIR} NAME)
    set(build_dir ${CMAKE_BINARY_DIR}/arduino/${sketch_stem})
    set(hex_file ${build_dir}/${sketch_stem}.ino.hex)

    add_custom_target(${sketch_stem}-compile
        COMMAND arduino-cli compile
            --fqbn ${ARDUINO_FQBN}
            --build-path ${build_dir}
            --library "${PROJECT_SOURCE_DIR}/src"
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
