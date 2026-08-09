#ifndef FOUNDATION_CONSTRUCTION_DEPOT_INTERNAL_H
#define FOUNDATION_CONSTRUCTION_DEPOT_INTERNAL_H

#include "foundation/construction_depot.h"

typedef struct {FactoryConstructionDepot *items;size_t count;size_t capacity;}
    FactoryConstructionDepotStore;
void factory_construction_depot_store_destroy(FactoryConstructionDepotStore *store);
bool factory_construction_depot_store_reserve_one(FactoryConstructionDepotStore *store);
void factory_construction_depot_store_add(FactoryConstructionDepotStore *store,
    FactoryEntityId id,int32_t x,int32_t y);
const FactoryConstructionDepot *factory_construction_depot_store_find(
    const FactoryConstructionDepotStore *store,FactoryEntityId id);
FactoryConstructionDepot *factory_construction_depot_store_find_mutable(
    FactoryConstructionDepotStore *store,FactoryEntityId id);
bool factory_construction_depot_store_remove(
    FactoryConstructionDepotStore *store,FactoryEntityId id);
#endif
