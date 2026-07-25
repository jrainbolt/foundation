#ifndef FOUNDATION_COMMAND_H
#define FOUNDATION_COMMAND_H

#include <stdbool.h>
#include <stdint.h>

#include "foundation/entity.h"
#include "foundation/assembler_recipe.h"
#include <foundation/world.h>
#include "foundation/recipe.h"

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
    FACTORY_COMMAND_SET_ASSEMBLER_RECIPE
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
} FactoryCommandResult;

/* Returns whether a command's enum fields are recognized. */
bool factory_command_is_well_formed(const FactoryCommand *command);

#endif
