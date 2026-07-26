#ifndef FOUNDATION_BURNER_INTERNAL_H
#define FOUNDATION_BURNER_INTERNAL_H

#include "foundation/burner.h"

typedef struct {
    FactoryEntityId owner_entity_id;
    FactoryFuelClassMask accepted_fuel_classes;
    FactoryItemType inventory_item;
    uint32_t inventory_quantity;
    FactoryItemType current_fuel_item;
    uint32_t remaining_burn_ticks;
    FactoryEnergy released_energy;
} FactoryBurner;

typedef struct {
    FactoryBurner *items;
    size_t count;
    size_t capacity;
} FactoryBurnerStore;

void factory_burner_store_destroy(FactoryBurnerStore *store);
bool factory_burner_store_reserve_one(FactoryBurnerStore *store);
void factory_burner_store_add(
    FactoryBurnerStore *store,
    FactoryEntityId owner,
    FactoryFuelClassMask accepted_classes
);
const FactoryBurner *factory_burner_store_find(
    const FactoryBurnerStore *store, FactoryEntityId owner
);
FactoryBurner *factory_burner_store_find_mutable(
    FactoryBurnerStore *store, FactoryEntityId owner
);
bool factory_burner_store_remove(
    FactoryBurnerStore *store, FactoryEntityId owner
);
bool factory_burner_can_accept(
    const FactoryBurner *burner, FactoryItemType item
);
bool factory_burner_insert(FactoryBurner *burner, FactoryItemType item);
bool factory_burner_consume_energy(
    FactoryBurner *burner, FactoryEnergy amount
);
bool factory_fuel_release_for_tick(
    const FactoryFuelDefinition *definition,
    uint32_t elapsed_ticks,
    FactoryEnergy *out_release
);
FactoryEnergy factory_burner_unreleased_energy(
    const FactoryBurner *burner
);
void factory_burner_store_begin_tick(
    FactoryBurnerStore *store, FactorySimulation *simulation
);
void factory_burner_store_finish_tick(
    FactoryBurnerStore *store, FactorySimulation *simulation
);

#endif
