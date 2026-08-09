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
#define FACTORY_RAIL_NETWORK_NONE 0U
#define FACTORY_RAIL_NEIGHBOR_COUNT 4U
#define FACTORY_LOCOMOTIVE_MOVE_TICKS 4U

typedef enum {
    FACTORY_LOCOMOTIVE_MOVING=0,
    FACTORY_LOCOMOTIVE_BLOCKED_TRACK,
    FACTORY_LOCOMOTIVE_BLOCKED_SWITCH,
    FACTORY_LOCOMOTIVE_BLOCKED_OCCUPIED,
    FACTORY_LOCOMOTIVE_DISCONNECTED
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
} FactoryLocomotiveInspection;

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
FactoryEntityId factory_simulation_get_rail_vehicle_occupant(
    const FactorySimulation *simulation,FactoryEntityId rail_entity_id);

#endif
