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
set(NLOHMANN_JSON_DIR "/usr/local/include/nlohmann") # JSON library path

# Output directory for binaries
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BIN_DIR})

# DataHandler library (shared by both client and server)
file(GLOB_RECURSE COMMON_SRC_FILES ${DATA_HANDLER_DIR}/*.cpp)
add_library(DataHandler ${COMMON_SRC_FILES})

# server target
file(GLOB_RECURSE SERVER_SRC_FILES ${SERVER_SRC_DIR}/*.cpp)
add_executable(server ${SERVER_SRC_FILES})

target_link_libraries(server PRIVATE DataHandler)
target_compile_definitions(server PRIVATE SERVER)
target_include_directories(server PRIVATE ${SERVER_INCLUDE_DIR} ${NLOHMANN_JSON_DIR})