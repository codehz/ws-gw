cmake_minimum_required(VERSION 3.15)

project(ws-gw)

set (CMAKE_CXX_STANDARD 20)

find_package(websocketpp CONFIG REQUIRED)

add_library(ws-gt src/service.cpp)
target_link_libraries(ws-gt PRIVATE websocketpp::websocketpp pthread)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  add_executable(test test.cpp)
  target_link_libraries(test PRIVATE websocketpp::websocketpp pthread ws-gt)
endif()