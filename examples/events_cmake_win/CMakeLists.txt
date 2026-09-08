cmake_minimum_required(VERSION 3.16)
project(ctx_example C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

set(CTX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../ctx")
set(CTX_EVENTS_DIR "${CTX_DIR}/events")

file(GLOB PROJECT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CTX_DIR}/*.c"
    "${CTX_EVENTS_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/ltx/*.c"
)

set(COMMON_C_FLAGS -Wall -g -O0)
set(COMMON_INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CTX_DIR}"
    "${CTX_EVENTS_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/ltx"
)

set(MAIN_OBJECTS)
foreach(source IN LISTS PROJECT_SOURCES)
    get_filename_component(source_name "${source}" NAME_WE)
    get_filename_component(source_dir "${source}" DIRECTORY)
    set(object_file "${CMAKE_CURRENT_BINARY_DIR}/${source_name}.o")

    add_custom_command(
        OUTPUT "${object_file}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}"
        COMMAND "${Python3_EXECUTABLE}" "${CTX_DIR}/coro_translater.py" "${source}"
        COMMAND "${CMAKE_C_COMPILER}" ${COMMON_C_FLAGS} "-I${CMAKE_CURRENT_SOURCE_DIR}" "-I${CTX_DIR}" "-I${CTX_EVENTS_DIR}" "-I${CMAKE_CURRENT_SOURCE_DIR}/ltx" -c "${source}" -o "${object_file}"
        DEPENDS "${source}" "${CTX_DIR}/coro_translater.py"
        COMMENT "Compiling ${source} with coro preprocessing"
        VERBATIM
    )

    list(APPEND MAIN_OBJECTS "${object_file}")
endforeach()

add_executable(main ${MAIN_OBJECTS})
target_include_directories(main PRIVATE ${COMMON_INCLUDE_DIRS})
set_target_properties(main PROPERTIES LINKER_LANGUAGE C)
