#ifndef FOUNDATION_REACTOR_H
#define FOUNDATION_REACTOR_H

#include <stddef.h>
#include <stdint.h>

#include "foundation/entity.h"
#include "foundation/heat.h"
#include "foundation/world.h"

typedef uint32_t FactoryNuclearFuelId;

#define FACTORY_NUCLEAR_FUEL_NONE 0U
#define FACTORY_NUCLEAR_FUEL_BASIC_ROD 1U
#define FACTORY_REACTOR_HEAT_CAPACITY UINT64_C(10000)
#define FACTORY_REACTOR_MAX_HEAT_OUTPUT_PER_TICK UINT64_C(100)
#define FACTORY_REACTOR_FUEL_INVENTORY_CAPACITY 1U

typedef struct {
    FactoryNuclearFuelId fuel_id;
    FactoryHeatQuantity total_heat_yield;
    uint32_t burn_duration_ticks;
    FactoryHeatQuantity maximum_heat_output_per_tick;
} FactoryNuclearFuelDefinition;

typedef enum {
    FACTORY_REACTOR_IDLE = 0,
    FACTORY_REACTOR_GENERATING,
    FACTORY_REACTOR_BLOCKED_HEAT_FULL
} FactoryReactorActivity;

typedef struct {
    FactoryEntityId entity_id;
    FactoryHeatQuantity stored_heat;
    FactoryHeatQuantity heat_capacity;
    FactoryNuclearFuelId inventory_fuel_id;
    uint32_t inventory_quantity;
    FactoryNuclearFuelId active_fuel_id;
    uint32_t remaining_burn_ticks;
    FactoryHeatQuantity remaining_heat_yield;
    FactoryHeatQuantity generated_last_tick;
    FactoryReactorActivity activity;
} FactoryReactorInspection;

typedef struct FactorySimulation FactorySimulation;

size_t factory_nuclear_fuel_definition_count(void);
/* Definition pointers refer to immutable static storage for program lifetime. */
const FactoryNuclearFuelDefinition *factory_nuclear_fuel_definition_at(
    size_t index);
const FactoryNuclearFuelDefinition *factory_nuclear_fuel_definition_get(
    FactoryNuclearFuelId fuel_id);
FactoryResult factory_simulation_get_reactor(
    const FactorySimulation *simulation,
    FactoryEntityId entity_id,
    FactoryReactorInspection *out_reactor);

#endif
