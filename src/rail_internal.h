#ifndef FOUNDATION_RAIL_INTERNAL_H
#define FOUNDATION_RAIL_INTERNAL_H

#include "foundation/rail.h"

typedef struct {FactoryEntityId entity_id;int32_t x,y;FactoryRailGeometry geometry;} FactoryRail;
typedef struct {FactoryRail *items;size_t count,capacity;} FactoryRailStore;
typedef struct {FactoryEntityId entity_id;int32_t x,y;FactoryDirection orientation;} FactoryRailStation;
typedef struct {FactoryRailStation *items;size_t count,capacity;} FactoryRailStationStore;
typedef struct {
    FactoryRailInspection *rails;size_t rail_count;
    FactoryRailStationInspection *stations;size_t station_count;
    FactoryRailNetworkInspection *networks;size_t network_count;
    bool dirty;
} FactoryRailTopology;

void factory_rail_store_destroy(FactoryRailStore *s);
bool factory_rail_store_reserve_one(FactoryRailStore *s);
void factory_rail_store_add(FactoryRailStore *s,FactoryEntityId id,int32_t x,
    int32_t y,FactoryRailGeometry geometry);
const FactoryRail *factory_rail_store_find(const FactoryRailStore *s,
    FactoryEntityId id);
bool factory_rail_store_remove(FactoryRailStore *s,FactoryEntityId id);
void factory_rail_station_store_destroy(FactoryRailStationStore *s);
bool factory_rail_station_store_reserve_one(FactoryRailStationStore *s);
void factory_rail_station_store_add(FactoryRailStationStore *s,
    FactoryEntityId id,int32_t x,int32_t y,FactoryDirection orientation);
const FactoryRailStation *factory_rail_station_store_find(
    const FactoryRailStationStore *s,FactoryEntityId id);
bool factory_rail_station_store_remove(FactoryRailStationStore *s,
    FactoryEntityId id);
void factory_rail_topology_destroy(FactoryRailTopology *t);
FactoryResult factory_rail_topology_rebuild(FactorySimulation *s);

#endif
