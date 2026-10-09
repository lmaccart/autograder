cmake_minimum_required(VERSION 3.16)
project(autograder CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # for editors (clangd)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(CURL REQUIRED)

find_package(nlohmann_json 3.9 QUIET)
if(NOT nlohmann_json_FOUND)
  message(STATUS "nlohmann_json not found; fetching it")
  include(FetchContent)
  FetchContent_Declare(json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
  FetchContent_MakeAvailable(json)
endif()

add_library(autograder_core STATIC
  src/config.cpp
  src/proc.cpp
  src/archive.cpp
  src/collect.cpp
  src/openai.cpp
  src/grader.cpp
  src/recurring.cpp
  src/report.cpp
  src/taildrop.cpp
  src/pipeline.cpp
  src/watch.cpp
  src/bundle.cpp
)
target_include_directories(autograder_core PUBLIC src)
target_link_libraries(autograder_core PUBLIC CURL::libcurl nlohmann_json::nlohmann_json)
target_compile_options(autograder_core PRIVATE -Wall -Wextra)

add_executable(autograder src/main.cpp)
target_link_libraries(autograder PRIVATE autograder_core)

install(TARGETS autograder RUNTIME DESTINATION bin)

enable_testing()
foreach(t test_recurring test_collect test_watch test_bundle)
  add_executable(${t} tests/${t}.cpp)
  target_link_libraries(${t} PRIVATE autograder_core)
  add_test(NAME ${t} COMMAND ${t})
endforeach()
