cmake_minimum_required(VERSION 3.10)
project(SimpleFileHost)
set(CMAKE_CXX_STANDARD 17)
set(SOURCE_FILES
    src/main.cpp
    src/cli.cpp
    src/server.cpp
    src/utils.cpp
)
find_package(Threads REQUIRED)
find_library(QRENCODE_LIB qrencode)
if(QRENCODE_LIB)
    message(STATUS "libqrencode found: ${QRENCODE_LIB}")
    add_definitions(-DHAVE_QRENCODE=1)
endif()
add_executable(simplefilehost ${SOURCE_FILES})
target_link_libraries(simplefilehost PRIVATE Threads::Threads ${QRENCODE_LIB})
