set(ARDUINO_PORT "/dev/ttyACM4" CACHE STRING "Upload port")

set(FQBN "teensy:avr:teensy41")
set(SKETCH_DIR ${CMAKE_CURRENT_SOURCE_DIR}/SeqnautDemo)

set(arduino_build_dir ${CMAKE_BINARY_DIR}/arduino)
set(hex_file ${arduino_build_dir}/SeqnautDemo.ino.hex)

add_custom_target(compile
    COMMAND arduino-cli compile --fqbn ${FQBN} --build-path ${arduino_build_dir}
        --library "${PROJECT_SOURCE_DIR}/src"
        ${SKETCH_DIR} --verbose
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    BYPRODUCTS ${hex_file}
    USES_TERMINAL
)

add_custom_target(upload
    COMMAND teensy_loader_cli --mcu=TEENSY41 -w ${hex_file}
    USES_TERMINAL
)

add_custom_target(monitor
    COMMAND arduino-cli monitor -p ${ARDUINO_PORT} -c baudrate=921600
    USES_TERMINAL
)
