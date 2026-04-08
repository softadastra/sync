cmake_minimum_required(VERSION 3.20)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    project(softadastra_sync
        VERSION 0.1.0
        DESCRIPTION "Softadastra Sync - Offline-first synchronization engine"
        LANGUAGES CXX
    )
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# ========================
# Standalone fallback options
# ========================
if(NOT DEFINED SOFTADASTRA_WARNINGS_AS_ERRORS)
    option(SOFTADASTRA_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
endif()

if(SOFTADASTRA_UMBRELLA_BUILD)
    set(SOFTADASTRA_SYNC_BUILD_TESTS ${SOFTADASTRA_BUILD_TESTS})
else()
    option(SOFTADASTRA_SYNC_BUILD_TESTS "Build sync tests" OFF)
endif()

option(SOFTADASTRA_SYNC_BUILD_EXAMPLES "Build sync examples" OFF)

# ========================
# Dependencies
# ========================
if(NOT SOFTADASTRA_UMBRELLA_BUILD)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.vix/vix_deps.cmake")
        include("${CMAKE_CURRENT_SOURCE_DIR}/.vix/vix_deps.cmake")
    endif()
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
    if(SOFTADASTRA_WARNINGS_AS_ERRORS)
        target_compile_options(softadastra_sync PRIVATE /WX)
    endif()
else()
    target_compile_options(softadastra_sync PRIVATE -Wall -Wextra -Wpedantic)
    if(SOFTADASTRA_WARNINGS_AS_ERRORS)
        target_compile_options(softadastra_sync PRIVATE -Werror)
    endif()
endif()

# ========================
# Target properties
# ========================
set_target_properties(softadastra_sync PROPERTIES
    OUTPUT_NAME softadastra_sync
    VERSION 0.1.0
    SOVERSION 0
    EXPORT_NAME sync
)

# ========================
# Install
# ========================
install(
    TARGETS softadastra_sync
    EXPORT softadastraSyncTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    EXPORT softadastraSyncTargets
    FILE softadastraSyncTargets.cmake
    NAMESPACE softadastra::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/softadastra_sync"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/softadastra_syncConfigVersion.cmake"
    VERSION 0.1.0
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/softadastra_syncConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/softadastra_syncConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/softadastra_sync"
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/softadastra_syncConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/softadastra_syncConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/softadastra_sync"
)

# ========================
# Examples
# ========================
if(SOFTADASTRA_SYNC_BUILD_EXAMPLES AND NOT SOFTADASTRA_UMBRELLA_BUILD)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/CMakeLists.txt")
        add_subdirectory(examples)
    else()
        message(STATUS "[sync] examples/ missing CMakeLists.txt -> skipping")
    endif()
endif()

# ========================
# Tests
# ========================
if(SOFTADASTRA_SYNC_BUILD_TESTS)
    include(CTest)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
        add_subdirectory(tests)
    else()
        message(STATUS "[sync] tests/ missing CMakeLists.txt -> skipping")
    endif()
endif()

# ========================
# Summary
# ========================
message(STATUS "------------------------------------------------------")
message(STATUS "softadastra::sync configured (0.1.0)")
message(STATUS "WAL target:      ${SOFTADASTRA_SYNC_WAL_TARGET}")
message(STATUS "Store target:    ${SOFTADASTRA_SYNC_STORE_TARGET}")
message(STATUS "Include dir:     ${CMAKE_CURRENT_SOURCE_DIR}/include")
message(STATUS "Build tests:     ${SOFTADASTRA_SYNC_BUILD_TESTS}")
message(STATUS "Build examples:  ${SOFTADASTRA_SYNC_BUILD_EXAMPLES}")
message(STATUS "Warnings=error:  ${SOFTADASTRA_WARNINGS_AS_ERRORS}")
message(STATUS "Build type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "Install prefix:  ${CMAKE_INSTALL_PREFIX}")
message(STATUS "------------------------------------------------------")
