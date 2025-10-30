cmake_minimum_required(VERSION 3.10)
project(SimpleFileHost)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)

option(NO_QRENCODE "Disable QR code support" OFF)

find_package(PkgConfig REQUIRED)

if(NOT NO_QRENCODE)
    pkg_check_modules(QRENCODE QUIET libqrencode)

    if(QRENCODE_FOUND)
        add_definitions(-DHAVE_QRENCODE)
        include_directories(${QRENCODE_INCLUDE_DIRS})
        message(STATUS "QR code support enabled")
    else()
        message(STATUS "QR code support disabled (libqrencode not found)")
    endif()
else()
    message(STATUS "QR code support disabled (by request)")
endif()

pkg_check_modules(LIBARCHIVE REQUIRED libarchive)

if(LIBARCHIVE_FOUND)
    message(STATUS "Found libarchive")
    include_directories(${LIBARCHIVE_INCLUDE_DIRS})
else()
    message(FATAL_ERROR "libarchive not found (required)")
endif()

find_package(OpenSSL REQUIRED)
if(OPENSSL_FOUND)
    message(STATUS "Found OpenSSL")
    include_directories(${OPENSSL_INCLUDE_DIR})
else()
    message(FATAL_ERROR "OpenSSL not found (required for TLS support)")
endif()

add_executable(simplefilehost
    src/main.cpp
    src/cli/cli.cpp
    src/server/server.cpp
    src/server/client_handler.cpp
    src/server/http_handlers.cpp
    src/server/file_transfer.cpp
    src/utils/utils.cpp
    src/utils/file_utils.cpp
    src/utils/network_utils.cpp
    src/utils/server_utils.cpp
    src/utils/archive_utils.cpp
    src/qr/qr_display.cpp
)

target_include_directories(simplefilehost PRIVATE
    src
    src/cli
    src/server
    src/utils
    src/qr
)

if(NOT NO_QRENCODE AND QRENCODE_FOUND)
    target_link_libraries(simplefilehost ${QRENCODE_LIBRARIES})
    message(STATUS "Linking with libqrencode")
endif()

target_link_libraries(simplefilehost OpenSSL::SSL OpenSSL::Crypto)

target_link_libraries(simplefilehost ${LIBARCHIVE_LIBRARIES})
target_link_libraries(simplefilehost pthread)
