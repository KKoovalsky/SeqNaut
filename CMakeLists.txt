cmake_minimum_required(VERSION 3.20)
project(SeqnautDemo NONE)

set(FQBN "teensy:avr:teensy41" CACHE STRING "Arduino board FQBN")
set(PORT "/dev/ttyACM4" CACHE STRING "Upload port")
set(SKETCH_DIR "${CMAKE_SOURCE_DIR}" CACHE PATH "Sketch directory")

add_custom_target(compile
    COMMAND arduino-cli compile --fqbn ${FQBN} ${SKETCH_DIR}
    WORKING_DIRECTORY ${SKETCH_DIR}
    USES_TERMINAL
)

add_custom_target(upload
    COMMAND arduino-cli upload -p ${PORT} --fqbn ${FQBN} ${SKETCH_DIR}
    WORKING_DIRECTORY ${SKETCH_DIR}
    USES_TERMINAL
)

add_custom_target(monitor
    COMMAND arduino-cli monitor -p ${PORT} -c baudrate=921600
    USES_TERMINAL
)
