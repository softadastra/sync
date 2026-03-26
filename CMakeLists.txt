cmake_minimum_required(VERSION 3.20)

project(softadastra_sync
    VERSION 0.1.0
    DESCRIPTION "Softadastra Sync - Offline-first synchronization engine"
    LANGUAGES CXX
)

include(GNUInstallDirs)

# ========================
# Options
# ========================
option(SOFTADASTRA_SYNC_BUILD_TESTS "Build sync tests" ON)
option(SOFTADASTRA_SYNC_BUILD_EXAMPLES "Build sync examples" ON)

# ========================
# Global settings
# ========================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ========================
# Dependencies
# ========================
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.vix/vix_deps.cmake")
    include("${CMAKE_CURRENT_SOURCE_DIR}/.vix/vix_deps.cmake")
endif()

if(NOT TARGET softadastra::wal AND NOT TARGET softadastra_wal)
    find_package(softadastra_wal CONFIG QUIET)
    find_package(wal CONFIG QUIET)
endif()

if(NOT TARGET softadastra::store AND NOT TARGET softadastra_store)
    find_package(softadastra_store CONFIG QUIET)
    find_package(store CONFIG QUIET)
endif()

set(SOFTADASTRA_SYNC_WAL_TARGET "")
if(TARGET softadastra::wal)
    set(SOFTADASTRA_SYNC_WAL_TARGET softadastra::wal)
elseif(TARGET softadastra_wal)
    set(SOFTADASTRA_SYNC_WAL_TARGET softadastra_wal)
endif()

set(SOFTADASTRA_SYNC_STORE_TARGET "")
if(TARGET softadastra::store)
    set(SOFTADASTRA_SYNC_STORE_TARGET softadastra::store)
elseif(TARGET softadastra_store)
    set(SOFTADASTRA_SYNC_STORE_TARGET softadastra_store)
endif()

if(SOFTADASTRA_SYNC_WAL_TARGET STREQUAL "")
    message(FATAL_ERROR
        "softadastra wal target not found.\n"
        "Install dependencies with Vix:\n"
        "  vix add @softadastra/wal\n"
        "  vix install"
    )
endif()

if(SOFTADASTRA_SYNC_STORE_TARGET STREQUAL "")
    message(FATAL_ERROR
        "softadastra store target not found.\n"
        "Install dependencies with Vix:\n"
        "  vix add @softadastra/store\n"
        "  vix install"
    )
endif()

# ========================
# Sources
# ========================
file(GLOB_RECURSE SOFTADASTRA_SYNC_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

# ========================
# Library
# ========================
add_library(softadastra_sync ${SOFTADASTRA_SYNC_SOURCES})
add_library(softadastra::sync ALIAS softadastra_sync)

target_compile_features(softadastra_sync PUBLIC cxx_std_20)

target_link_libraries(softadastra_sync
    PUBLIC
        ${SOFTADASTRA_SYNC_WAL_TARGET}
        ${SOFTADASTRA_SYNC_STORE_TARGET}
)

# ========================
# Include directories
# ========================
target_include_directories(softadastra_sync
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# ========================
# Compiler options
# ========================
if(MSVC)
    target_compile_options(softadastra_sync PRIVATE /W4 /permissive-)
else()
    target_compile_options(softadastra_sync PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ========================
# Target properties
# ========================
set_target_properties(softadastra_sync PROPERTIES
    OUTPUT_NAME softadastra_sync
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
    EXPORT_NAME sync
)

# ========================
# Examples
# ========================
if(SOFTADASTRA_SYNC_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# ========================
# Tests
# ========================
if(SOFTADASTRA_SYNC_BUILD_TESTS)
    include(CTest)
    add_subdirectory(tests)
endif()

# ========================
# Summary
# ========================
message(STATUS "------------------------------------------------------")
message(STATUS "softadastra::sync configured (${PROJECT_VERSION})")
message(STATUS "WAL target:      ${SOFTADASTRA_SYNC_WAL_TARGET}")
message(STATUS "Store target:    ${SOFTADASTRA_SYNC_STORE_TARGET}")
message(STATUS "Include dir:     ${CMAKE_CURRENT_SOURCE_DIR}/include")
message(STATUS "Build tests:     ${SOFTADASTRA_SYNC_BUILD_TESTS}")
message(STATUS "Build examples:  ${SOFTADASTRA_SYNC_BUILD_EXAMPLES}")
message(STATUS "Build type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "------------------------------------------------------")
