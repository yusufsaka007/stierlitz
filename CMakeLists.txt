cmake_minimum_required(VERSION 3.10)

project(StierlitzServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_BUILD_TYPE Debug) # DEBUG

# For Server
set(SERVER_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/Server/include)
set(SERVER_SRC_DIR ${CMAKE_SOURCE_DIR}/Server/src)
set(BIN_DIR ${CMAKE_SOURCE_DIR}/bin)
set(NLOHMANN_JSON_DIR "/usr/local/include/nlohmann")

include_directories(${SERVER_INCLUDE_DIR})
include_directories(${NLOHMANN_JSON_DIR})

file(GLOB_RECURSE SERVER_SRC_FILES ${SERVER_SRC_DIR}/*.cpp)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BIN_DIR})
add_executable(StierlitzServer ${SERVER_SRC_FILES})