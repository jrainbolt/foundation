#ifndef FOUNDATION_RESEARCH_INTERNAL_H
#define FOUNDATION_RESEARCH_INTERNAL_H

#include "foundation/research.h"

typedef struct {
    uint32_t completed_units;
    uint64_t work_ticks;
    bool science_committed;
} FactoryTechnologyProgress;

typedef struct {
    FactoryTechnologyId active;
    uint64_t completed_bits;
    uint32_t science_quantity;
    FactoryTechnologyProgress progress[FACTORY_TECHNOLOGY_COUNT];
} FactoryResearchState;

FactoryResult factory_research_select(struct FactorySimulation *simulation,
    FactoryTechnologyId id);
FactoryResult factory_research_insert_science(struct FactorySimulation *simulation,
    uint32_t quantity);
void factory_research_update(struct FactorySimulation *simulation);
bool factory_research_state_valid(const FactoryResearchState *state);

#endif
