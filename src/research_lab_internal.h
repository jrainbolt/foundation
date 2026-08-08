#ifndef FOUNDATION_RESEARCH_LAB_INTERNAL_H
#define FOUNDATION_RESEARCH_LAB_INTERNAL_H

#include "foundation/research_lab.h"

#include <stddef.h>

typedef struct {
    FactoryEntityId entity_id;
    int32_t x;
    int32_t y;
    uint32_t science_quantity;
    FactoryResearchLabActivity activity;
    uint32_t science_consumed_last_tick;
    uint32_t work_contributed_last_tick;
} FactoryResearchLab;

typedef struct {
    FactoryResearchLab *items;
    size_t count;
    size_t capacity;
} FactoryResearchLabStore;

void factory_research_lab_store_destroy(FactoryResearchLabStore *store);
bool factory_research_lab_store_reserve_one(FactoryResearchLabStore *store);
void factory_research_lab_store_add(FactoryResearchLabStore *store,
    FactoryEntityId id,int32_t x,int32_t y);
const FactoryResearchLab *factory_research_lab_store_find(
    const FactoryResearchLabStore *store,FactoryEntityId id);
FactoryResearchLab *factory_research_lab_store_find_mutable(
    FactoryResearchLabStore *store,FactoryEntityId id);
bool factory_research_lab_store_remove(
    FactoryResearchLabStore *store,FactoryEntityId id);
void factory_research_labs_update(FactorySimulation *simulation);

#endif
