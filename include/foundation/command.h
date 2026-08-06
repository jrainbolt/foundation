#ifndef FOUNDATION_COMMAND_H
#define FOUNDATION_COMMAND_H

#include <stdbool.h>
#include <stdint.h>

#include "foundation/entity.h"
#include "foundation/assembler_recipe.h"
#include "foundation/fluid.h"
#include "foundation/reactor.h"
#include <foundation/world.h>
#include "foundation/recipe.h"
#include "foundation/research.h"

#define FACTORY_COMMAND_QUEUE_CAPACITY 32U

typedef enum {
    FACTORY_DIRECTION_NORTH = 0,
    FACTORY_DIRECTION_EAST,
    FACTORY_DIRECTION_SOUTH,
    FACTORY_DIRECTION_WEST
} FactoryDirection;

typedef enum {
    FACTORY_COMMAND_PLACE_EXTRACTOR = 0,
    FACTORY_COMMAND_PLACE_BELT,
    FACTORY_COMMAND_PLACE_STORAGE,
    FACTORY_COMMAND_PLACE_REFINERY,
    FACTORY_COMMAND_SET_REFINERY_RECIPE,
    FACTORY_COMMAND_PLACE_ASSEMBLER,
    FACTORY_COMMAND_DEMOLISH_ENTITY,
    FACTORY_COMMAND_PLACE_SPLITTER,
    FACTORY_COMMAND_PLACE_INSERTER,
    FACTORY_COMMAND_GRANT_CONSTRUCTION_UNITS,
    FACTORY_COMMAND_SET_ASSEMBLER_RECIPE,
    FACTORY_COMMAND_SET_STORAGE_OUTPUT,
    FACTORY_COMMAND_PLACE_POWER_POLE,
    FACTORY_COMMAND_PLACE_POWER_GENERATOR,
    FACTORY_COMMAND_PLACE_FLUID_TANK,
    FACTORY_COMMAND_FLUID_INSERT,
    FACTORY_COMMAND_FLUID_REMOVE,
    FACTORY_COMMAND_FLUID_TRANSFER,
    FACTORY_COMMAND_PLACE_PIPE,
    FACTORY_COMMAND_PLACE_WATER_EXTRACTOR,
    FACTORY_COMMAND_PLACE_BOILER,
    FACTORY_COMMAND_PLACE_STEAM_ENGINE,
    FACTORY_COMMAND_PLACE_SOLAR_GENERATOR,
    FACTORY_COMMAND_PLACE_ACCUMULATOR,
    FACTORY_COMMAND_PLACE_REACTOR_CORE,
    FACTORY_COMMAND_INSERT_REACTOR_FUEL,
    FACTORY_COMMAND_PLACE_HEAT_CONDUCTOR,
    FACTORY_COMMAND_PLACE_HEAT_EXCHANGER,
    FACTORY_COMMAND_PLACE_STEAM_TURBINE,
    FACTORY_COMMAND_PLACE_STEAM_CONDENSER,
    FACTORY_COMMAND_SELECT_RESEARCH,
    FACTORY_COMMAND_INSERT_RESEARCH_SCIENCE
} FactoryCommandType;

typedef struct {
    FactoryCommandType type;
    union {
        struct {
            int32_t x;
            int32_t y;
            FactoryDirection output_direction;
        } place_extractor;
        struct {
            int32_t x;
            int32_t y;
            FactoryDirection direction;
        } place_belt;
        struct {
            int32_t x;
            int32_t y;
        } place_storage;
        struct {
            int32_t x;
            int32_t y;
            FactoryDirection input_direction;
            FactoryDirection output_direction;
        } place_refinery;
        struct {
            FactoryEntityId refinery_entity;
            FactoryRecipeId recipe_id;
        } set_refinery_recipe;
        struct {
            int32_t x;
            int32_t y;
            FactoryDirection output_direction;
        } place_assembler;
        struct {
            FactoryEntityId entity_id;
        } demolish_entity;
        struct {
            int32_t x;
            int32_t y;
            FactoryDirection facing;
        } place_splitter;
        struct {
            int32_t x;
            int32_t y;
            FactoryDirection facing;
        } place_inserter;
        struct {
            uint32_t amount;
        } grant_construction_units;
        struct {
            FactoryEntityId assembler_entity;
            FactoryAssemblerRecipeId recipe_id;
        } set_assembler_recipe;
        struct {
            FactoryEntityId storage_entity;
            FactoryItemType item;
        } set_storage_output;
        struct {
            int32_t x;
            int32_t y;
        } place_power_pole;
        struct {
            int32_t x;
            int32_t y;
        } place_power_generator;
        struct {
            int32_t x;
            int32_t y;
        } place_fluid_tank;
        struct {
            FactoryEntityId destination_entity_id;
            FactoryFluidType fluid_type;
            FactoryFluidQuantity quantity;
        } fluid_insert;
        struct {
            FactoryEntityId source_entity_id;
            FactoryFluidQuantity quantity;
        } fluid_remove;
        struct {
            FactoryEntityId source_entity_id;
            FactoryEntityId destination_entity_id;
            FactoryFluidQuantity quantity;
        } fluid_transfer;
        struct {
            int32_t x;
            int32_t y;
        } place_pipe;
        struct { int32_t x; int32_t y; } place_water_extractor;
        struct { int32_t x; int32_t y; } place_boiler;
        struct { int32_t x; int32_t y; } place_steam_engine;
        struct { int32_t x; int32_t y; } place_solar_generator;
        struct { int32_t x; int32_t y; } place_accumulator;
        struct { int32_t x; int32_t y; } place_reactor_core;
        struct {
            FactoryEntityId reactor_entity_id;
            FactoryNuclearFuelId fuel_id;
        } insert_reactor_fuel;
        struct { int32_t x; int32_t y; } place_heat_conductor;
        struct { int32_t x; int32_t y; } place_heat_exchanger;
        struct { int32_t x; int32_t y; } place_steam_turbine;
        struct { int32_t x; int32_t y; } place_steam_condenser;
        struct { FactoryTechnologyId technology_id; } select_research;
        struct { uint32_t quantity; } insert_research_science;
    } data;
} FactoryCommand;

typedef struct {
    FactoryCommand command;
    FactoryResult result;
    FactoryEntityId entity_id;
    FactoryEntityType entity_type;
    int32_t x;
    int32_t y;
    uint32_t construction_units_changed;
    uint32_t construction_units_remaining;
    FactoryAssemblerRecipeId previous_assembler_recipe;
    FactoryAssemblerRecipeId new_assembler_recipe;
    FactoryItemType previous_storage_output;
    FactoryItemType new_storage_output;
} FactoryCommandResult;

/* Returns whether a command's enum fields are recognized. */
bool factory_command_is_well_formed(const FactoryCommand *command);

#endif
