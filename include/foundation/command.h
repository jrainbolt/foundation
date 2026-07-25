#ifndef FOUNDATION_COMMAND_H
#define FOUNDATION_COMMAND_H

#include <stdbool.h>
#include <stdint.h>

#include "foundation/entity.h"
#include "factory/world.h"
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
    FACTORY_COMMAND_SET_REFINERY_RECIPE
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
    } data;
} FactoryCommand;

typedef struct {
    FactoryCommand command;
    FactoryResult result;
    FactoryEntityId entity_id;
} FactoryCommandResult;

/* Returns whether a command's enum fields are recognized. */
bool factory_command_is_well_formed(const FactoryCommand *command);

#endif
