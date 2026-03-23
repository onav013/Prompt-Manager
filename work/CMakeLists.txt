cmake_minimum_required(VERSION 3.16)
project(prompt_manager_c C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(prompt_manager
    src/cli.c
    src/input_utils.c
    src/style.c
    src/ui_frame.c
    src/ui_icons.c
    src/main.c
    src/prompt_store.c
    src/llm_client.c
)

target_include_directories(prompt_manager PRIVATE include)

if (WIN32)
    target_compile_options(prompt_manager PRIVATE /utf-8)
    target_link_libraries(prompt_manager PRIVATE winhttp)
endif ()

enable_testing()

add_executable(prompt_store_tests
    tests/test_prompt_store.c
    src/prompt_store.c
)
target_include_directories(prompt_store_tests PRIVATE include)

if (WIN32)
    target_compile_options(prompt_store_tests PRIVATE /utf-8)
endif ()

add_test(NAME prompt_store_tests COMMAND prompt_store_tests)
