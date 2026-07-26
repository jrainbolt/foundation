#ifndef FOUNDATION_BURNER_H
#define FOUNDATION_BURNER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "foundation/entity.h"
#include "foundation/item.h"
#include "foundation/world.h"

typedef uint32_t FactoryFuelClassMask;
typedef uint64_t FactoryEnergy;

#define FACTORY_FUEL_CLASS_SOLID (1U << 0U)
#define FACTORY_BURNER_RELEASED_ENERGY_CAPACITY UINT64_C(100000)

typedef struct {
    FactoryItemType item_type;
    uint32_t burn_duration_ticks;
    FactoryEnergy energy;
    FactoryFuelClassMask fuel_class;
} FactoryFuelDefinition;

typedef struct {
    FactoryEntityId owner_entity_id;
    FactoryFuelClassMask accepted_fuel_classes;
    FactoryItemType inventory_item;
    uint32_t inventory_quantity;
    FactoryItemType current_fuel_item;
    uint32_t remaining_burn_ticks;
    uint32_t total_burn_duration_ticks;
    FactoryEnergy unreleased_fuel_energy;
    FactoryEnergy released_energy;
    bool active;
} FactoryBurnerInspection;

typedef struct FactorySimulation FactorySimulation;

size_t factory_fuel_definition_count(void);
/* Returned fuel-definition pointers refer to immutable process-lifetime data. */
const FactoryFuelDefinition *factory_fuel_definition_at(size_t index);
const FactoryFuelDefinition *factory_fuel_definition_get(
    FactoryItemType item
);
bool factory_fuel_definition_is_valid(
    const FactoryFuelDefinition *definition
);

FactoryResult factory_simulation_get_burner(
    const FactorySimulation *simulation,
    FactoryEntityId owner_entity_id,
    FactoryBurnerInspection *out_burner
);

/*
 * Inspection is copied by value. released_energy is electrical energy already
 * made available to the owning machine; unreleased_fuel_energy still belongs
 * to the active item and cannot be consumed. An inactive burner reports zero
 * duration, remaining ticks, and unreleased energy.
 */

#endif
