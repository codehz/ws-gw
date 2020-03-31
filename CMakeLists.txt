cmake_minimum_required(VERSION 3.15)

project(ws-gw)

set(CMAKE_CXX_STANDARD 20)


find_package(websocketpp CONFIG REQUIRED)
find_package(Flatbuffers CONFIG REQUIRED)
find_package(Threads REQUIRED)

if(WIN32)
  add_definitions(-DNOMINMAX -DBOOST_ASIO_DISABLE_CO_AWAIT -D_SILENCE_CLANG_COROUTINE_MESSAGE)
endif()

add_library(ws-gt src/service.cpp)
target_include_directories(ws-gt PUBLIC include)
target_link_libraries(ws-gt PUBLIC websocketpp::websocketpp flatbuffers::flatbuffers Threads::Threads)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  add_executable(test test.cpp)
  target_link_libraries(test PRIVATE ws-gt)
endif()