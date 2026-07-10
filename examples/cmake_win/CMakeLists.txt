cmake_minimum_required(VERSION 3.16)
project(ctx_example C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

file(GLOB CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/ctx/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/ctx/events/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/ltx/*.c"
)

set(COMMON_C_FLAGS -Wall -g -O0)
set(COMMON_INCLUDE_DIRS
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -I${CMAKE_CURRENT_SOURCE_DIR}/ctx
    -I${CMAKE_CURRENT_SOURCE_DIR}/ctx/events
    -I${CMAKE_CURRENT_SOURCE_DIR}/ltx
)

set(MAIN_OBJECTS)
foreach(source IN LISTS CONFIGURE_DEPENDS)
    get_filename_component(source_name "${source}" NAME_WE)
    get_filename_component(source_dir "${source}" DIRECTORY)
    set(object_file "${CMAKE_CURRENT_BINARY_DIR}/${source_name}.o")

    add_custom_command(
        OUTPUT "${object_file}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/ctx/coro_translater.py" "${source}"
        COMMAND "${CMAKE_C_COMPILER}" ${COMMON_C_FLAGS} ${COMMON_INCLUDE_DIRS} -c "${source}" -o "${object_file}"
        DEPENDS "${source}" "${CMAKE_CURRENT_SOURCE_DIR}/ctx/coro_translater.py"
        COMMENT "Compiling ${source} with coro preprocessing"
        VERBATIM
    )

    list(APPEND MAIN_OBJECTS "${object_file}")
endforeach()

add_executable(main ${MAIN_OBJECTS})
set_target_properties(main PROPERTIES LINKER_LANGUAGE C)
