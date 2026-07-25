cmake_minimum_required(VERSION 3.16)

project(factory_engine VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(factory_engine
    src/belt.c
    src/command.c
    src/entity.c
    src/extractor.c
    src/item.c
    src/recipe.c
    src/refinery.c
    src/simulation.c
    src/storage.c
    src/world.c
)
target_include_directories(factory_engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(factory_engine PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_executable(factory_console_demo demo/console_demo.c)
target_link_libraries(factory_console_demo PRIVATE factory_engine)
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(factory_console_demo PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(factory_world_tests tests/test_world.c)
    target_link_libraries(factory_world_tests PRIVATE factory_engine)
    if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(factory_world_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME factory_world_tests COMMAND factory_world_tests)

    add_executable(factory_entity_tests tests/test_entity.c)
    target_link_libraries(factory_entity_tests PRIVATE factory_engine)
    if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(factory_entity_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME factory_entity_tests COMMAND factory_entity_tests)

    add_executable(factory_command_tests tests/test_command.c)
    target_link_libraries(factory_command_tests PRIVATE factory_engine)
    if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(factory_command_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME factory_command_tests COMMAND factory_command_tests)

    add_executable(factory_extractor_tests tests/test_extractor.c)
    target_link_libraries(factory_extractor_tests PRIVATE factory_engine)
    if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(factory_extractor_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME factory_extractor_tests COMMAND factory_extractor_tests)

    add_executable(factory_belt_tests tests/test_belt.c)
    target_link_libraries(factory_belt_tests PRIVATE factory_engine)
    if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(factory_belt_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME factory_belt_tests COMMAND factory_belt_tests)

    add_executable(factory_storage_tests tests/test_storage.c)
    target_link_libraries(factory_storage_tests PRIVATE factory_engine)
    if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(factory_storage_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME factory_storage_tests COMMAND factory_storage_tests)

    add_executable(factory_logistics_tests tests/test_logistics.c)
    target_link_libraries(factory_logistics_tests PRIVATE factory_engine)
    if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(factory_logistics_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME factory_logistics_tests COMMAND factory_logistics_tests)

    add_executable(factory_recipe_tests tests/test_recipe.c)
    target_link_libraries(factory_recipe_tests PRIVATE factory_engine)
    if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(factory_recipe_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME factory_recipe_tests COMMAND factory_recipe_tests)

    add_executable(factory_refinery_tests tests/test_refinery.c)
    target_link_libraries(factory_refinery_tests PRIVATE factory_engine)
    if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(factory_refinery_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME factory_refinery_tests COMMAND factory_refinery_tests)

    add_executable(factory_processing_tests tests/test_processing.c)
    target_link_libraries(factory_processing_tests PRIVATE factory_engine)
    if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(factory_processing_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME factory_processing_tests COMMAND factory_processing_tests)

    add_executable(factory_copper_tests tests/test_copper.c)
    target_link_libraries(factory_copper_tests PRIVATE factory_engine)
    if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(factory_copper_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME factory_copper_tests COMMAND factory_copper_tests)

    add_executable(factory_simulation_tests tests/test_simulation.c)
    target_link_libraries(factory_simulation_tests PRIVATE factory_engine)
    if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(factory_simulation_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME factory_simulation_tests COMMAND factory_simulation_tests)
endif()
