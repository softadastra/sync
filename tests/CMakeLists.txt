cmake_minimum_required(VERSION 3.20)

set(SYNC_TEST_SOURCES
    test_ack.cpp
    test_applier.cpp
    test_conflict.cpp
    test_engine.cpp
    test_outbox.cpp
    test_queue.cpp
    test_recovery.cpp
    test_sync_operation.cpp
)

foreach(test_source IN LISTS SYNC_TEST_SOURCES)
    get_filename_component(test_file ${test_source} NAME_WE)
    set(target_name softadastra_sync_${test_file})

    add_executable(${target_name}
        ${CMAKE_CURRENT_SOURCE_DIR}/${test_source}
    )

    target_compile_features(${target_name} PRIVATE cxx_std_20)

    target_link_libraries(${target_name}
        PRIVATE
            softadastra::sync
    )

    target_include_directories(${target_name}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )

    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME ${target_name} COMMAND ${target_name})
endforeach()
