cmake_minimum_required(VERSION 3.20)

project(softadastra_sync
    VERSION 0.1.0
    DESCRIPTION "Softadastra Sync - Offline-first synchronization engine"
    LANGUAGES CXX
)

# ========================
# Options
# ========================
option(SOFTADASTRA_SYNC_BUILD_TESTS "Build sync tests" ON)
option(SOFTADASTRA_SYNC_BUILD_EXAMPLES "Build sync examples" ON)

# ========================
# Library
# ========================
add_library(softadastra_sync)

target_compile_features(softadastra_sync PUBLIC cxx_std_20)

# ========================
# Dependencies
# ========================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.vix/vix_deps.cmake)
    include(${CMAKE_CURRENT_SOURCE_DIR}/.vix/vix_deps.cmake)
endif()

if(NOT TARGET softadastra::wal)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../wal/CMakeLists.txt)
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../wal build_wal)
    else()
        message(FATAL_ERROR
            "softadastra::wal not found. "
            "Install dependencies with Vix or provide ../wal in the monorepo."
        )
    endif()
endif()

if(NOT TARGET softadastra::store)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../store/CMakeLists.txt)
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../store build_store)
    else()
        message(FATAL_ERROR
            "softadastra::store not found. "
            "Install dependencies with Vix or provide ../store in the monorepo."
        )
    endif()
endif()

target_link_libraries(softadastra_sync
    PUBLIC
        softadastra::wal
        softadastra::store
)

# ========================
# Include directories
# ========================
target_include_directories(softadastra_sync
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# ========================
# Sources
# ========================
file(GLOB_RECURSE SYNC_SOURCES
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

target_sources(softadastra_sync
    PRIVATE
        ${SYNC_SOURCES}
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
# Position Independent Code
# ========================
set_target_properties(softadastra_sync PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

# ========================
# Namespacing
# ========================
add_library(softadastra::sync ALIAS softadastra_sync)

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
