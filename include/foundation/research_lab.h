#ifndef FOUNDATION_RESEARCH_LAB_H
#define FOUNDATION_RESEARCH_LAB_H

#include <stdbool.h>
#include <stdint.h>

#include "foundation/entity.h"
#include "foundation/power.h"
#include <foundation/world.h>

#define FACTORY_RESEARCH_LAB_SCIENCE_CAPACITY 100U
#define FACTORY_POWER_DEMAND_RESEARCH_LAB 50U

typedef enum {
    FACTORY_RESEARCH_LAB_IDLE = 0,
    FACTORY_RESEARCH_LAB_WORKING,
    FACTORY_RESEARCH_LAB_NO_ACTIVE_RESEARCH,
    FACTORY_RESEARCH_LAB_UNPOWERED,
    FACTORY_RESEARCH_LAB_NO_SCIENCE,
    FACTORY_RESEARCH_LAB_WAITING
} FactoryResearchLabActivity;

typedef struct {
    FactoryEntityId entity_id;
    int32_t x;
    int32_t y;
    uint32_t science_quantity;
    uint32_t science_capacity;
    FactoryPowerNetworkId power_network_id;
    bool connected;
    bool powered;
    FactoryResearchLabActivity activity;
    uint32_t science_consumed_last_tick;
    uint32_t work_contributed_last_tick;
} FactoryResearchLabInspection;

typedef struct FactorySimulation FactorySimulation;
FactoryResult factory_simulation_get_research_lab(
    const FactorySimulation *simulation,FactoryEntityId entity_id,
    FactoryResearchLabInspection *out_lab);

#endif
