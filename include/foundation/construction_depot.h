#ifndef FOUNDATION_CONSTRUCTION_DEPOT_H
#define FOUNDATION_CONSTRUCTION_DEPOT_H

#include <stdbool.h>
#include <stdint.h>

#include "foundation/simulation.h"

#define FACTORY_CONSTRUCTION_DEPOT_CAPACITY 500U
#define FACTORY_CONSTRUCTION_DEPOT_RADIUS 8U

typedef struct {
    FactoryEntityId entity_id;
    int32_t x;
    int32_t y;
    uint32_t material_quantity;
} FactoryConstructionDepot;

bool factory_simulation_get_construction_depot(
    const FactorySimulation *simulation,FactoryEntityId entity_id,
    FactoryConstructionDepot *out_depot);
bool factory_simulation_position_has_construction_coverage(
    const FactorySimulation *simulation,int32_t x,int32_t y,
    FactoryEntityId *out_depot_id,bool *out_has_complete_supply);

#endif
