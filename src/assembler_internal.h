#ifndef FOUNDATION_ASSEMBLER_INTERNAL_H
#define FOUNDATION_ASSEMBLER_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "foundation/assembler.h"
#include "foundation/power.h"

typedef struct {
    FactoryAssembler *items;
    size_t count;
    size_t capacity;
} FactoryAssemblerStore;

void factory_assembler_store_destroy(FactoryAssemblerStore *store);
bool factory_assembler_store_reserve_one(FactoryAssemblerStore *store);
void factory_assembler_store_add(
    FactoryAssemblerStore *store,
    FactoryEntityId id,
    int32_t x,
    int32_t y,
    FactoryDirection output_direction
);
const FactoryAssembler *factory_assembler_store_find(
    const FactoryAssemblerStore *store,
    FactoryEntityId id
);
FactoryAssembler *factory_assembler_store_find_mutable(
    FactoryAssemblerStore *store,
    FactoryEntityId id
);
bool factory_assembler_store_remove(
    FactoryAssemblerStore *store,
    FactoryEntityId entity_id
);
void factory_assembler_store_update(
    FactoryAssemblerStore *store,
    const FactorySimulation *simulation
);
bool factory_assembler_configure_recipe(
    FactoryAssembler *assembler,
    FactoryAssemblerRecipeId recipe_id
);

#endif
