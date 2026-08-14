#ifndef FOUNDATION_RAIL_H
#define FOUNDATION_RAIL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "foundation/command.h"

typedef struct FactorySimulation FactorySimulation;

typedef enum {
    FACTORY_RAIL_HORIZONTAL = 0,
    FACTORY_RAIL_VERTICAL,
    FACTORY_RAIL_CURVE_NE,
    FACTORY_RAIL_CURVE_NW,
    FACTORY_RAIL_CURVE_SE,
    FACTORY_RAIL_CURVE_SW,
    FACTORY_RAIL_GEOMETRY_COUNT
} FactoryRailGeometry;

typedef enum {
    FACTORY_RAIL_SWITCH_STEM_NORTH = 0,
    FACTORY_RAIL_SWITCH_STEM_SOUTH,
    FACTORY_RAIL_SWITCH_STEM_EAST,
    FACTORY_RAIL_SWITCH_STEM_WEST,
    FACTORY_RAIL_SWITCH_GEOMETRY_COUNT
} FactoryRailSwitchGeometry;

typedef enum {
    FACTORY_RAIL_SWITCH_BRANCH_A = 0,
    FACTORY_RAIL_SWITCH_BRANCH_B,
    FACTORY_RAIL_SWITCH_BRANCH_COUNT
} FactoryRailSwitchBranch;

enum {
    FACTORY_RAIL_PORT_NORTH = 1U << 0U,
    FACTORY_RAIL_PORT_EAST = 1U << 1U,
    FACTORY_RAIL_PORT_SOUTH = 1U << 2U,
    FACTORY_RAIL_PORT_WEST = 1U << 3U
};

typedef FactoryEntityId FactoryRailNetworkId;
typedef FactoryEntityId FactoryTrainId;
typedef FactoryEntityId FactoryRailBlockId;
#define FACTORY_RAIL_NETWORK_NONE 0U
#define FACTORY_TRAIN_NONE 0U
#define FACTORY_RAIL_BLOCK_NONE 0U
#define FACTORY_RAIL_NEIGHBOR_COUNT 4U
#define FACTORY_LOCOMOTIVE_MOVE_TICKS 4U
#define FACTORY_CARGO_WAGON_CAPACITY 100U
#define FACTORY_RAIL_STATION_FREIGHT_CAPACITY 200U
#define FACTORY_RAIL_STATION_TRANSFER_QUANTITY 10U
#define FACTORY_TRAIN_ROUTE_MAX_STEPS 4096U

typedef enum {
    FACTORY_TRAIN_ROUTE_NONE = 0,
    FACTORY_TRAIN_ROUTE_ACTIVE,
    FACTORY_TRAIN_ROUTE_ARRIVED,
    FACTORY_TRAIN_ROUTE_INVALID
} FactoryTrainRouteStatus;

typedef enum {
    FACTORY_TRAIN_RESERVATION_NONE = 0,
    FACTORY_TRAIN_RESERVATION_HELD,
    FACTORY_TRAIN_RESERVATION_WAITING,
    FACTORY_TRAIN_RESERVATION_INVALID
} FactoryTrainReservationStatus;

typedef enum {
    FACTORY_RAIL_SIGNAL_GREEN = 0,
    FACTORY_RAIL_SIGNAL_RED,
    FACTORY_RAIL_SIGNAL_RESERVED
} FactoryRailSignalAspect;

typedef enum {
    FACTORY_TRAIN_CHAIN_NONE=0,
    FACTORY_TRAIN_CHAIN_HELD,
    FACTORY_TRAIN_CHAIN_WAITING,
    FACTORY_TRAIN_CHAIN_INVALID
} FactoryTrainChainStatus;

typedef enum {
    FACTORY_RAIL_STATION_FREIGHT_DISABLED=0,
    FACTORY_RAIL_STATION_FREIGHT_LOAD,
    FACTORY_RAIL_STATION_FREIGHT_UNLOAD
} FactoryRailStationFreightMode;

typedef enum {
    FACTORY_RAIL_FREIGHT_NONE=0,
    FACTORY_RAIL_FREIGHT_LOADING,
    FACTORY_RAIL_FREIGHT_UNLOADING
} FactoryRailFreightActivity;

typedef struct {
    FactoryEntityId rail_entity_id;
    FactoryDirection entry_direction;
} FactoryTrainRouteStep;

typedef enum {
    FACTORY_LOCOMOTIVE_MOVING=0,
    FACTORY_LOCOMOTIVE_BLOCKED_TRACK,
    FACTORY_LOCOMOTIVE_BLOCKED_SWITCH,
    FACTORY_LOCOMOTIVE_BLOCKED_OCCUPIED,
    FACTORY_LOCOMOTIVE_DISCONNECTED,
    FACTORY_LOCOMOTIVE_ARRIVED,
    FACTORY_LOCOMOTIVE_ROUTE_INVALID,
    FACTORY_LOCOMOTIVE_BLOCKED_RESERVATION
} FactoryLocomotiveActivity;

typedef struct {
    FactoryEntityId entity_id;
    FactoryEntityId rail_entity_id;
    int32_t x,y;
    FactoryDirection entry_direction;
    FactoryDirection travel_direction;
    uint32_t movement_progress;
    uint32_t movement_interval;
    FactoryEntityId next_rail_id;
    FactoryRailNetworkId network_id;
    FactoryLocomotiveActivity activity;
    FactoryTrainId train_id;
    uint32_t vehicle_count;
    FactoryEntityId destination_station_id;
    FactoryTrainRouteStatus route_status;
    uint32_t route_length;
    uint32_t route_index;
    FactoryEntityId next_planned_rail_id;
    FactoryRailBlockId current_block_id;
    FactoryRailBlockId next_route_block_id;
    FactoryRailBlockId reserved_block_id;
    FactoryTrainReservationStatus reservation_status;
    FactoryTrainId blocking_train_id;
    uint32_t reserved_block_count;
    uint32_t chain_required_block_count;
    FactoryRailBlockId blocking_block_id;
    FactoryTrainChainStatus chain_status;
} FactoryLocomotiveInspection;

typedef struct {
    FactoryRailBlockId block_id;
    uint32_t member_count;
    FactoryTrainId reserved_train_id;
    uint32_t occupied_train_count;
    bool switch_boundary;
    bool station_boundary;
    bool endpoint_boundary;
} FactoryRailBlockInspection;

/*
 * Orientation is the controlled travel direction. The attached rail is one
 * cell to the signal's left when viewed along that direction, leaving the
 * signal beside the track. Its upstream rail is the attached rail's neighbor
 * opposite the controlled direction.
 */
typedef struct {
    FactoryEntityId entity_id;
    int32_t x,y;
    FactoryDirection orientation;
    FactoryEntityId attached_rail_id;
    FactoryEntityId upstream_rail_id;
    FactoryRailBlockId upstream_block_id;
    FactoryRailBlockId downstream_block_id;
    FactoryRailSignalAspect aspect;
    FactoryTrainId reserved_train_id;
    uint32_t occupied_train_count;
    bool connected;
} FactoryRailSignalInspection;

typedef FactoryRailSignalInspection FactoryRailChainSignalInspection;

typedef struct {
    FactoryEntityId entity_id;
    FactoryEntityId rail_entity_id;
    int32_t x,y;
    FactoryDirection entry_direction;
    FactoryRailNetworkId network_id;
    FactoryTrainId train_id;
    uint32_t consist_index;
    bool coupled;
    FactoryItemType cargo_item;
    uint32_t cargo_quantity;
    uint32_t cargo_capacity;
} FactoryCargoWagonInspection;

typedef struct {
    FactoryEntityId entity_id;
    int32_t x;
    int32_t y;
    FactoryRailGeometry geometry;
    uint32_t port_mask;
    uint32_t connection_mask;
    FactoryEntityId neighbors[FACTORY_RAIL_NEIGHBOR_COUNT];
    FactoryRailNetworkId network_id;
    uint32_t connection_count;
} FactoryRailInspection;

typedef struct {
    FactoryEntityId entity_id;
    int32_t x;
    int32_t y;
    FactoryRailSwitchGeometry geometry;
    FactoryRailSwitchBranch selected_branch;
    FactoryDirection stem_direction;
    FactoryDirection branch_a_direction;
    FactoryDirection branch_b_direction;
    uint32_t port_mask;
    uint32_t connection_mask;
    FactoryEntityId neighbors[FACTORY_RAIL_NEIGHBOR_COUNT];
    FactoryRailNetworkId network_id;
    uint32_t connection_count;
} FactoryRailSwitchInspection;

typedef struct {
    bool allowed;
    FactoryDirection exit_direction;
    FactoryEntityId exit_entity_id;
} FactoryRailTraversal;

typedef struct {
    FactoryEntityId entity_id;
    int32_t x;
    int32_t y;
    FactoryDirection orientation;
    FactoryEntityId attached_rail_id;
    FactoryRailNetworkId network_id;
    bool connected;
    FactoryRailStationFreightMode freight_mode;
    FactoryItemType configured_item;
    uint32_t freight_quantity;
    uint32_t freight_capacity;
    FactoryTrainId eligible_train_id;
    bool freight_transfer_possible;
    uint32_t latest_transfer_quantity;
    FactoryRailFreightActivity latest_transfer_activity;
} FactoryRailStationInspection;

typedef struct {
    FactoryRailNetworkId network_id;
    uint32_t rail_count;
    uint32_t switch_count;
    uint32_t station_count;
} FactoryRailNetworkInspection;

uint32_t factory_rail_geometry_port_mask(FactoryRailGeometry geometry);
bool factory_rail_geometry_is_valid(FactoryRailGeometry geometry);
bool factory_rail_switch_geometry_is_valid(FactoryRailSwitchGeometry geometry);
bool factory_rail_switch_branch_is_valid(FactoryRailSwitchBranch branch);
uint32_t factory_rail_switch_geometry_port_mask(
    FactoryRailSwitchGeometry geometry);
bool factory_rail_switch_geometry_directions(FactoryRailSwitchGeometry geometry,
    FactoryDirection *out_stem,FactoryDirection *out_branch_a,
    FactoryDirection *out_branch_b);
bool factory_simulation_get_rail(const FactorySimulation *simulation,
    FactoryEntityId id,FactoryRailInspection *out_rail);
bool factory_simulation_get_rail_station(const FactorySimulation *simulation,
    FactoryEntityId id,FactoryRailStationInspection *out_station);
bool factory_simulation_get_rail_switch(const FactorySimulation *simulation,
    FactoryEntityId id,FactoryRailSwitchInspection *out_switch);
bool factory_simulation_get_rail_traversal(const FactorySimulation *simulation,
    FactoryEntityId rail_entity_id,FactoryDirection entry_direction,
    FactoryRailTraversal *out_traversal);
size_t factory_simulation_get_rail_network_count(
    const FactorySimulation *simulation);
const FactoryRailNetworkInspection *factory_simulation_get_rail_network(
    const FactorySimulation *simulation,size_t index);
bool factory_simulation_get_locomotive(const FactorySimulation *simulation,
    FactoryEntityId id,FactoryLocomotiveInspection *out_locomotive);
bool factory_simulation_get_cargo_wagon(const FactorySimulation *simulation,
    FactoryEntityId id,FactoryCargoWagonInspection *out_wagon);
FactoryResult factory_simulation_cargo_wagon_insert(FactorySimulation *simulation,
    FactoryEntityId id,FactoryItemType item,uint32_t quantity);
FactoryResult factory_simulation_cargo_wagon_remove(FactorySimulation *simulation,
    FactoryEntityId id,FactoryItemType item,uint32_t quantity);
FactoryEntityId factory_simulation_get_rail_vehicle_occupant(
    const FactorySimulation *simulation,FactoryEntityId rail_entity_id);
bool factory_simulation_get_train_route_step(const FactorySimulation *simulation,
    FactoryTrainId train_id,size_t index,FactoryTrainRouteStep *out_step);
size_t factory_simulation_get_rail_block_count(const FactorySimulation *simulation);
bool factory_simulation_get_rail_block_at(const FactorySimulation *simulation,
    size_t index,FactoryRailBlockInspection *out_block);
FactoryRailBlockId factory_simulation_get_rail_block_for_rail(
    const FactorySimulation *simulation,FactoryEntityId rail_entity_id);
bool factory_simulation_get_rail_block_member(const FactorySimulation *simulation,
    FactoryRailBlockId block_id,size_t index,FactoryEntityId *out_rail_id);
bool factory_simulation_get_rail_signal(const FactorySimulation *simulation,
    FactoryEntityId id,FactoryRailSignalInspection *out_signal);
bool factory_simulation_get_rail_chain_signal(const FactorySimulation *simulation,
    FactoryEntityId id,FactoryRailChainSignalInspection *out_signal);
/*
 * The simulation owns this ordered future-block sequence. The count and indexed
 * values remain valid until the next mutating simulation call. Out-of-range
 * access, a missing train, or a null output returns false.
 */
size_t factory_simulation_get_train_reserved_block_count(
    const FactorySimulation *simulation,FactoryTrainId train_id);
bool factory_simulation_get_train_reserved_block_at(
    const FactorySimulation *simulation,FactoryTrainId train_id,size_t index,
    FactoryRailBlockId *out_block_id);

#endif
