cmake_minimum_required(VERSION 3.10)

project(Stierlitz)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_BUILD_TYPE Debug) # DEBUG

# Directories
set(SERVER_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/Server/include)
set(SERVER_SRC_DIR ${CMAKE_SOURCE_DIR}/Server/src)
set(DATA_HANDLER_DIR ${CMAKE_SOURCE_DIR}/DataHandler)
set(BIN_DIR ${CMAKE_SOURCE_DIR}/bin)

set(C2_SCRIPT_PATH ${CMAKE_SOURCE_DIR}/URXVT/c2_terminal.py)
set(C2_FIFO_PATH ${CMAKE_SOURCE_DIR}/URXVT/fifo/c2_fifo)

# Output directory for binaries
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BIN_DIR})

# DataHandler library (shared by both client and server)
file(GLOB_RECURSE COMMON_SRC_FILES ${DATA_HANDLER_DIR}/*.cpp)
add_library(DataHandlerServer ${COMMON_SRC_FILES})
target_compile_definitions(DataHandlerServer PRIVATE SERVER)

# server target
file(GLOB_RECURSE SERVER_SRC_FILES ${SERVER_SRC_DIR}/*.cpp)
add_executable(server ${SERVER_SRC_FILES})
target_compile_definitions(server PRIVATE SERVER
    C2_SCRIPT_PATH="${C2_SCRIPT_PATH}"
    C2_FIFO_PATH="${C2_FIFO_PATH}"
)

target_link_libraries(server PRIVATE DataHandlerServer)
target_include_directories(server PRIVATE ${SERVER_INCLUDE_DIR} ${DATA_HANDLER_DIR})