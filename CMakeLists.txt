cmake_minimum_required(VERSION 3.16)

project(factory_engine VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(FOUNDATION_ENABLE_ASAN "Enable AddressSanitizer instrumentation" OFF)
option(FOUNDATION_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer instrumentation" OFF)

add_library(foundation_project_options INTERFACE)

if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(foundation_project_options INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )

    if(FOUNDATION_ENABLE_ASAN)
        target_compile_options(foundation_project_options INTERFACE
            -fsanitize=address
            -fno-omit-frame-pointer
        )
        target_link_options(foundation_project_options INTERFACE
            -fsanitize=address
        )
    endif()

    if(FOUNDATION_ENABLE_UBSAN)
        target_compile_options(foundation_project_options INTERFACE
            -fsanitize=undefined
            -fno-omit-frame-pointer
        )
        target_link_options(foundation_project_options INTERFACE
            -fsanitize=undefined
        )
    endif()
elseif(FOUNDATION_ENABLE_ASAN OR FOUNDATION_ENABLE_UBSAN)
    message(FATAL_ERROR
        "Foundation sanitizers require a compiler compatible with GCC or Clang flags"
    )
endif()

add_library(factory_engine
    src/accumulator.c
    src/assembler.c
    src/assembler_recipe.c
    src/belt.c
    src/burner.c
    src/clock.c
    src/command.c
    src/construction.c
    src/construction_inventory.c
    src/entity.c
    src/event.c
    src/extractor.c
    src/fluid.c
    src/fluid_machine.c
    src/item.c
    src/inserter.c
    src/logistics_endpoint.c
    src/power.c
    src/presentation.c
    src/recipe.c
    src/refinery.c
    src/simulation.c
    src/solar.c
    src/snapshot.c
    src/steam_engine.c
    src/splitter.c
    src/storage.c
    src/world.c
)
target_include_directories(factory_engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(factory_engine PUBLIC foundation_project_options)

add_executable(factory_console_demo demo/console_demo.c)
target_link_libraries(factory_console_demo PRIVATE factory_engine)

include(CTest)
if(BUILD_TESTING)
    add_executable(factory_world_tests tests/test_world.c)
    target_link_libraries(factory_world_tests PRIVATE factory_engine)
    add_test(NAME factory_world_tests COMMAND factory_world_tests)

    add_executable(factory_entity_tests tests/test_entity.c)
    target_link_libraries(factory_entity_tests PRIVATE factory_engine)
    add_test(NAME factory_entity_tests COMMAND factory_entity_tests)

    add_executable(factory_command_tests tests/test_command.c)
    target_link_libraries(factory_command_tests PRIVATE factory_engine)
    add_test(NAME factory_command_tests COMMAND factory_command_tests)

    add_executable(factory_extractor_tests tests/test_extractor.c)
    target_link_libraries(factory_extractor_tests PRIVATE factory_engine)
    add_test(NAME factory_extractor_tests COMMAND factory_extractor_tests)

    add_executable(factory_belt_tests tests/test_belt.c)
    target_link_libraries(factory_belt_tests PRIVATE factory_engine)
    add_test(NAME factory_belt_tests COMMAND factory_belt_tests)

    add_executable(factory_storage_tests tests/test_storage.c)
    target_link_libraries(factory_storage_tests PRIVATE factory_engine)
    add_test(NAME factory_storage_tests COMMAND factory_storage_tests)

    add_executable(factory_storage_output_tests tests/test_storage_output.c)
    target_include_directories(
        factory_storage_output_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(factory_storage_output_tests PRIVATE factory_engine)
    add_test(NAME factory_storage_output_tests COMMAND factory_storage_output_tests)

    add_executable(factory_logistics_tests tests/test_logistics.c)
    target_link_libraries(factory_logistics_tests PRIVATE factory_engine)
    add_test(NAME factory_logistics_tests COMMAND factory_logistics_tests)

    add_executable(factory_recipe_tests tests/test_recipe.c)
    target_link_libraries(factory_recipe_tests PRIVATE factory_engine)
    add_test(NAME factory_recipe_tests COMMAND factory_recipe_tests)

    add_executable(factory_refinery_tests tests/test_refinery.c)
    target_link_libraries(factory_refinery_tests PRIVATE factory_engine)
    add_test(NAME factory_refinery_tests COMMAND factory_refinery_tests)

    add_executable(factory_processing_tests tests/test_processing.c)
    target_link_libraries(factory_processing_tests PRIVATE factory_engine)
    add_test(NAME factory_processing_tests COMMAND factory_processing_tests)

    add_executable(factory_copper_tests tests/test_copper.c)
    target_link_libraries(factory_copper_tests PRIVATE factory_engine)
    add_test(NAME factory_copper_tests COMMAND factory_copper_tests)

    add_executable(factory_assembler_tests tests/test_assembler.c)
    target_link_libraries(factory_assembler_tests PRIVATE factory_engine)
    add_test(NAME factory_assembler_tests COMMAND factory_assembler_tests)

    add_executable(
        factory_assembler_recipe_tests
        tests/test_assembler_recipes.c
    )
    target_include_directories(
        factory_assembler_recipe_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(factory_assembler_recipe_tests PRIVATE factory_engine)
    add_test(
        NAME factory_assembler_recipe_tests
        COMMAND factory_assembler_recipe_tests
    )

    add_executable(factory_component_pipeline_tests tests/test_component_pipeline.c)
    target_link_libraries(factory_component_pipeline_tests PRIVATE factory_engine)
    add_test(NAME factory_component_pipeline_tests COMMAND factory_component_pipeline_tests)

    add_executable(factory_demolition_tests tests/test_demolition.c)
    target_link_libraries(factory_demolition_tests PRIVATE factory_engine)
    add_test(NAME factory_demolition_tests COMMAND factory_demolition_tests)

    add_executable(factory_splitter_tests tests/test_splitter.c)
    target_link_libraries(factory_splitter_tests PRIVATE factory_engine)
    add_test(NAME factory_splitter_tests COMMAND factory_splitter_tests)

    add_executable(factory_inserter_tests tests/test_inserter.c)
    target_link_libraries(factory_inserter_tests PRIVATE factory_engine)
    add_test(NAME factory_inserter_tests COMMAND factory_inserter_tests)

    add_executable(
        factory_logistics_endpoint_tests
        tests/test_logistics_endpoint.c
    )
    target_include_directories(
        factory_logistics_endpoint_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(
        factory_logistics_endpoint_tests
        PRIVATE
            factory_engine
    )
    add_test(
        NAME factory_logistics_endpoint_tests
        COMMAND factory_logistics_endpoint_tests
    )

    add_executable(factory_construction_tests tests/test_construction.c)
    target_include_directories(
        factory_construction_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(factory_construction_tests PRIVATE factory_engine)
    add_test(
        NAME factory_construction_tests
        COMMAND factory_construction_tests
    )

    add_executable(factory_simulation_tests tests/test_simulation.c)
    target_link_libraries(factory_simulation_tests PRIVATE factory_engine)
    add_test(NAME factory_simulation_tests COMMAND factory_simulation_tests)

    add_executable(factory_snapshot_tests tests/test_snapshot.c)
    target_include_directories(
        factory_snapshot_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(factory_snapshot_tests PRIVATE factory_engine)
    add_test(NAME factory_snapshot_tests COMMAND factory_snapshot_tests)

    add_executable(factory_power_tests tests/test_power.c)
    target_include_directories(
        factory_power_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(factory_power_tests PRIVATE factory_engine)
    add_test(NAME factory_power_tests COMMAND factory_power_tests)

    add_executable(factory_event_tests tests/test_event.c)
    target_include_directories(
        factory_event_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(factory_event_tests PRIVATE factory_engine)
    add_test(NAME factory_event_tests COMMAND factory_event_tests)

    add_executable(factory_presentation_tests tests/test_presentation.c)
    target_include_directories(
        factory_presentation_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(factory_presentation_tests PRIVATE factory_engine)
    add_test(
        NAME factory_presentation_tests
        COMMAND factory_presentation_tests
    )

    add_executable(factory_burner_tests tests/test_burner.c)
    target_include_directories(
        factory_burner_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(factory_burner_tests PRIVATE factory_engine)
    add_test(NAME factory_burner_tests COMMAND factory_burner_tests)

    add_executable(factory_fluid_tests tests/test_fluid.c)
    target_include_directories(
        factory_fluid_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(factory_fluid_tests PRIVATE factory_engine)
    add_test(NAME factory_fluid_tests COMMAND factory_fluid_tests)

    add_executable(factory_fluid_network_tests tests/test_fluid_network.c)
    target_link_libraries(factory_fluid_network_tests PRIVATE factory_engine)
    add_test(
        NAME factory_fluid_network_tests COMMAND factory_fluid_network_tests
    )

    add_executable(factory_fluid_machine_tests tests/test_fluid_machine.c)
    target_include_directories(
        factory_fluid_machine_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(factory_fluid_machine_tests PRIVATE factory_engine)
    add_test(
        NAME factory_fluid_machine_tests COMMAND factory_fluid_machine_tests
    )

    add_executable(factory_steam_engine_tests tests/test_steam_engine.c)
    target_include_directories(
        factory_steam_engine_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(factory_steam_engine_tests PRIVATE factory_engine)
    add_test(NAME factory_steam_engine_tests COMMAND factory_steam_engine_tests)

    add_executable(factory_solar_tests tests/test_solar.c)
    target_include_directories(
        factory_solar_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(factory_solar_tests PRIVATE factory_engine)
    add_test(NAME factory_solar_tests COMMAND factory_solar_tests)

    add_executable(factory_accumulator_tests tests/test_accumulator.c)
    target_include_directories(
        factory_accumulator_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(factory_accumulator_tests PRIVATE factory_engine)
    add_test(NAME factory_accumulator_tests COMMAND factory_accumulator_tests)
endif()
