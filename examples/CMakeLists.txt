cmake_minimum_required(VERSION 3.20)

set(SYNC_EXAMPLE_SOURCES
    basic_sync.cpp
    remote_apply.cpp
    retry.cpp
)

foreach(example_source IN LISTS SYNC_EXAMPLE_SOURCES)
    get_filename_component(example_file ${example_source} NAME_WE)
    set(target_name softadastra_sync_example_${example_file})

    add_executable(${target_name}
        ${CMAKE_CURRENT_SOURCE_DIR}/${example_source}
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
endforeach()
