cmake_minimum_required(VERSION 3.15)

project(ws-gw)

set (CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS ON)

find_package(websocketpp CONFIG REQUIRED)
find_package(Flatbuffers CONFIG REQUIRED)
find_package(Threads REQUIRED)

if(WIN32)
  add_definitions(-DNOMINMAX -DBOOST_ASIO_DISABLE_CO_AWAIT -D_SILENCE_CLANG_COROUTINE_MESSAGE -D_SILENCE_CLANG_CONCEPTS_MESSAGE)
  add_compile_options(/EHsc)
endif()

add_library(ws-gw SHARED src/service.cpp)
target_include_directories(ws-gw PUBLIC include)
target_link_libraries(ws-gw PUBLIC websocketpp::websocketpp flatbuffers::flatbuffers Threads::Threads)
if(WIN32)
  target_compile_definitions(ws-gw PRIVATE -DWsGwImpl)
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  add_executable(test test.cpp)
  target_link_libraries(test PRIVATE ws-gw)
endif()