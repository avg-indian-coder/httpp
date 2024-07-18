cmake_minimum_required(VERSION 3.13)
project(http-server-starter-cpp)

find_package(ZLIB REQUIRED)
if (ZLIB_FOUND)
    include_directories(${ZLIB_INCLUDE_DIRS})
endif()

set(ZLIB_TEST_SOURCES src/server.cpp)
set(CMAKE_CXX_STANDARD 20) # Enable the C++20 standard

file(GLOB_RECURSE SOURCE_FILES src/*.cpp)
add_executable(${PROJECT_NAME} ${ZLIB_TEST_SOURCES})
# target_link_libraries(${PROJECT_NAME} ${ZLIB_LIBRARIES})
target_link_libraries(${PROJECT_NAME} PRIVATE ZLIB::ZLIB)
add_executable(server ${SOURCE_FILES})
