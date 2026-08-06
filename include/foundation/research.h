#ifndef FOUNDATION_RESEARCH_H
#define FOUNDATION_RESEARCH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "foundation/item.h"
#include <foundation/world.h>

typedef uint32_t FactoryTechnologyId;

enum {
    FACTORY_TECHNOLOGY_NONE = 0U,
    FACTORY_TECHNOLOGY_BASIC_AUTOMATION = 1U,
    FACTORY_TECHNOLOGY_FLUID_HANDLING = 2U,
    FACTORY_TECHNOLOGY_COUNT = 2U,
    FACTORY_TECHNOLOGY_MAX_PREREQUISITES = 2U
};

typedef uint64_t FactoryUnlockFlags;
enum {
    FACTORY_UNLOCK_NONE = 0U,
    FACTORY_UNLOCK_AUTOMATION = UINT64_C(1) << 0,
    FACTORY_UNLOCK_FLUID_HANDLING = UINT64_C(1) << 1,
    FACTORY_UNLOCK_ALL = FACTORY_UNLOCK_AUTOMATION
        | FACTORY_UNLOCK_FLUID_HANDLING
};

typedef struct {
    FactoryTechnologyId id;
    FactoryTechnologyId prerequisites[FACTORY_TECHNOLOGY_MAX_PREREQUISITES];
    uint32_t prerequisite_count;
    FactoryItemType science_item;
    uint32_t science_quantity_per_unit;
    uint32_t required_science_units;
    uint64_t work_ticks_per_unit;
    FactoryUnlockFlags unlock_flags;
} FactoryTechnologyDefinition;

typedef struct {
    FactoryTechnologyId technology_id;
    uint32_t completed_units;
    uint32_t required_units;
    uint64_t work_ticks_in_current_unit;
    uint64_t work_ticks_per_unit;
    bool science_committed_for_current_unit;
    bool completed;
} FactoryTechnologyProgressInspection;

size_t factory_technology_definition_count(void);
const FactoryTechnologyDefinition *factory_technology_definition_at(size_t index);
const FactoryTechnologyDefinition *factory_technology_definition_get(
    FactoryTechnologyId id);
bool factory_technology_definitions_validate(
    const FactoryTechnologyDefinition *definitions,size_t count);

typedef struct FactorySimulation FactorySimulation;
FactoryTechnologyId factory_simulation_get_active_research(
    const FactorySimulation *simulation);
FactoryResult factory_simulation_get_technology_progress(
    const FactorySimulation *simulation,FactoryTechnologyId technology_id,
    FactoryTechnologyProgressInspection *out_progress);
bool factory_simulation_is_technology_completed(
    const FactorySimulation *simulation,FactoryTechnologyId technology_id);
bool factory_simulation_has_unlock(
    const FactorySimulation *simulation,FactoryUnlockFlags unlock);
uint32_t factory_simulation_get_completed_technology_count(
    const FactorySimulation *simulation);
uint32_t factory_simulation_get_research_science_quantity(
    const FactorySimulation *simulation);

#endif
