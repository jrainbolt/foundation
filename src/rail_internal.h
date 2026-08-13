#ifndef FOUNDATION_RAIL_INTERNAL_H
#define FOUNDATION_RAIL_INTERNAL_H

#include "foundation/rail.h"

typedef struct {FactoryEntityId entity_id;int32_t x,y;FactoryRailGeometry geometry;} FactoryRail;
typedef struct {FactoryRail *items;size_t count,capacity;} FactoryRailStore;
typedef struct {FactoryEntityId entity_id;int32_t x,y;FactoryDirection orientation;} FactoryRailStation;
typedef struct {FactoryRailStation *items;size_t count,capacity;} FactoryRailStationStore;
typedef struct {FactoryEntityId entity_id;int32_t x,y;
    FactoryRailSwitchGeometry geometry;FactoryRailSwitchBranch selected_branch;}
    FactoryRailSwitch;
typedef struct {FactoryRailSwitch *items;size_t count,capacity;}
    FactoryRailSwitchStore;
typedef struct {FactoryEntityId entity_id,rail_entity_id;FactoryDirection entry_direction;
    uint32_t progress;FactoryLocomotiveActivity activity;
    FactoryEntityId rear_vehicle_id;uint32_t vehicle_count;
    FactoryEntityId destination_station_id;FactoryTrainRouteStatus route_status;
    FactoryTrainRouteStep *route;size_t route_length,route_index;
    FactoryRailBlockId reserved_block_id;FactoryTrainReservationStatus reservation_status;
    FactoryTrainId blocking_train_id;} FactoryLocomotive;
typedef struct {FactoryEntityId entity_id,rail_entity_id;FactoryDirection entry_direction;
    FactoryTrainId train_id;FactoryEntityId previous_vehicle_id,next_vehicle_id;
    FactoryItemType cargo_item;uint32_t cargo_quantity;} FactoryCargoWagon;
typedef struct {FactoryCargoWagon *items;size_t count,capacity;} FactoryCargoWagonStore;
typedef struct {FactoryEntityId id,from,to;FactoryDirection next_entry;
    bool eligible,move;FactoryLocomotiveActivity blocked;} FactoryLocomotivePlan;
typedef struct {FactoryLocomotive *items;size_t count,capacity;
    FactoryLocomotivePlan *plans;size_t plan_capacity;} FactoryLocomotiveStore;
typedef struct {
    FactoryRailBlockId block_id;size_t member_offset,member_count;
    bool switch_boundary,station_boundary,endpoint_boundary;
} FactoryRailBlock;
typedef struct {FactoryEntityId rail_id;FactoryRailBlockId block_id;}
    FactoryRailBlockMember;
typedef struct {
    FactoryRailInspection *rails;size_t rail_count;
    FactoryRailSwitchInspection *switches;size_t switch_count;
    FactoryRailStationInspection *stations;size_t station_count;
    FactoryRailNetworkInspection *networks;size_t network_count;
    FactoryRailBlock *blocks;size_t block_count;
    FactoryRailBlockMember *block_members;size_t block_member_count;
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
void factory_rail_switch_store_destroy(FactoryRailSwitchStore *s);
bool factory_rail_switch_store_reserve_one(FactoryRailSwitchStore *s);
void factory_rail_switch_store_add(FactoryRailSwitchStore *s,
    FactoryEntityId id,int32_t x,int32_t y,FactoryRailSwitchGeometry geometry);
const FactoryRailSwitch *factory_rail_switch_store_find(
    const FactoryRailSwitchStore *s,FactoryEntityId id);
FactoryRailSwitch *factory_rail_switch_store_find_mutable(
    FactoryRailSwitchStore *s,FactoryEntityId id);
bool factory_rail_switch_store_remove(FactoryRailSwitchStore *s,
    FactoryEntityId id);
void factory_rail_topology_destroy(FactoryRailTopology *t);
FactoryResult factory_rail_topology_rebuild(FactorySimulation *s);
void factory_locomotive_store_destroy(FactoryLocomotiveStore *s);
bool factory_locomotive_store_reserve(FactoryLocomotiveStore *s,size_t required);
FactoryLocomotive *factory_locomotive_store_find_mutable(
    FactoryLocomotiveStore *s,FactoryEntityId id);
const FactoryLocomotive *factory_locomotive_store_find(
    const FactoryLocomotiveStore *s,FactoryEntityId id);
bool factory_locomotive_store_remove(FactoryLocomotiveStore *s,FactoryEntityId id);
FactoryResult factory_train_set_destination(FactorySimulation *s,
    FactoryTrainId train_id,FactoryEntityId station_id,bool replan);
FactoryResult factory_train_clear_destination(FactorySimulation *s,
    FactoryTrainId train_id);
bool factory_train_routes_validate(const FactorySimulation *s);
void factory_cargo_wagon_store_destroy(FactoryCargoWagonStore *s);
bool factory_cargo_wagon_store_reserve(FactoryCargoWagonStore *s,size_t required);
FactoryCargoWagon *factory_cargo_wagon_store_find_mutable(
    FactoryCargoWagonStore *s,FactoryEntityId id);
const FactoryCargoWagon *factory_cargo_wagon_store_find(
    const FactoryCargoWagonStore *s,FactoryEntityId id);
bool factory_cargo_wagon_store_remove(FactoryCargoWagonStore *s,FactoryEntityId id);
void factory_locomotives_update(FactorySimulation *simulation);
void factory_train_reservations_update(FactorySimulation *simulation);
bool factory_train_reservations_validate(const FactorySimulation *simulation);

#endif
