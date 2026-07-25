#ifndef FOUNDATION_ASSEMBLER_H
#define FOUNDATION_ASSEMBLER_H

#include <stdbool.h>
#include <stdint.h>

#include "foundation/assembler_recipe.h"
#include "foundation/command.h"

typedef struct {
    FactoryItemType item;
    uint32_t count;
    uint32_t capacity;
} FactoryAssemblerInputSlot;

typedef struct {
    FactoryEntityId entity_id;
    int32_t x;
    int32_t y;
    FactoryDirection output_direction;
    FactoryAssemblerRecipeId recipe_id;
    FactoryAssemblerInputSlot
        input_slots[FACTORY_ASSEMBLER_MAX_INPUT_TYPES];
    FactoryItemType output_item;
    uint32_t output_amount;
    uint32_t processing_progress;
    uint32_t processing_duration;
    bool processing;
} FactoryAssembler;

#endif
