cmake_minimum_required(VERSION 3.16)
project(reliable-remote-backup-system 
    VERSION 1.0.0
    DESCRIPTION "Reliable Remote Backup System with UDP/TCP"
    LANGUAGES CXX
)

# C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Build type defaults to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Export compile commands for IDE integration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# =============================================================================
# Dependencies
# =============================================================================

# FetchContent for downloading dependencies
include(FetchContent)

# spdlog for logging
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.12.0
)
FetchContent_MakeAvailable(spdlog)

# nlohmann/json for JSON support
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)

# cpp-httplib for HTTP server (Phase B)
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.14.3
)
FetchContent_MakeAvailable(httplib)

# GoogleTest for unit testing
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# =============================================================================
# Common Library
# =============================================================================
add_library(common STATIC
    src/common/utils.cpp
    src/common/error_codes.cpp
    src/common/file_utils.cpp
    src/common/logger.cpp
    src/common/metrics.cpp
)
target_include_directories(common PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(common PUBLIC spdlog::spdlog)

# =============================================================================
# Server Executable
# =============================================================================
add_executable(backup-server
    src/server/main.cpp
    src/server/server.cpp
    src/server/command_handler.cpp
    src/server/http_server.cpp
)
target_link_libraries(backup-server PRIVATE common spdlog::spdlog nlohmann_json::nlohmann_json httplib::httplib)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(backup-server PRIVATE ws2_32)
endif()

# =============================================================================
# Client Executable
# =============================================================================
add_executable(backup-client
    src/client/main.cpp
    src/client/client.cpp
)
target_link_libraries(backup-client PRIVATE common spdlog::spdlog)

if(WIN32)
    target_link_libraries(backup-client PRIVATE ws2_32)
endif()

# =============================================================================
# Tests
# =============================================================================
enable_testing()

add_executable(unit_tests
    tests/test_main.cpp
    tests/test_utils.cpp
    tests/test_protocol.cpp
    tests/test_error_codes.cpp
    tests/test_metrics.cpp
)
target_link_libraries(unit_tests PRIVATE 
    common 
    GTest::gtest 
    GTest::gtest_main
    spdlog::spdlog
)

include(GoogleTest)
gtest_discover_tests(unit_tests)

# =============================================================================
# Installation
# =============================================================================
install(TARGETS backup-server backup-client
    RUNTIME DESTINATION bin
)

# Install static files for web UI
install(DIRECTORY ${CMAKE_SOURCE_DIR}/static/
    DESTINATION share/${PROJECT_NAME}/static
)

# Copy static files to build directory for development
file(COPY ${CMAKE_SOURCE_DIR}/static DESTINATION ${CMAKE_BINARY_DIR})

# =============================================================================
# CPack Configuration (optional packaging)
# =============================================================================
set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
