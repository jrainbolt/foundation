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

enum {
    FACTORY_RAIL_PORT_NORTH = 1U << 0U,
    FACTORY_RAIL_PORT_EAST = 1U << 1U,
    FACTORY_RAIL_PORT_SOUTH = 1U << 2U,
    FACTORY_RAIL_PORT_WEST = 1U << 3U
};

typedef FactoryEntityId FactoryRailNetworkId;
#define FACTORY_RAIL_NETWORK_NONE 0U
#define FACTORY_RAIL_NEIGHBOR_COUNT 4U

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
    FactoryDirection orientation;
    FactoryEntityId attached_rail_id;
    FactoryRailNetworkId network_id;
    bool connected;
} FactoryRailStationInspection;

typedef struct {
    FactoryRailNetworkId network_id;
    uint32_t rail_count;
    uint32_t station_count;
} FactoryRailNetworkInspection;

uint32_t factory_rail_geometry_port_mask(FactoryRailGeometry geometry);
bool factory_rail_geometry_is_valid(FactoryRailGeometry geometry);
bool factory_simulation_get_rail(const FactorySimulation *simulation,
    FactoryEntityId id,FactoryRailInspection *out_rail);
bool factory_simulation_get_rail_station(const FactorySimulation *simulation,
    FactoryEntityId id,FactoryRailStationInspection *out_station);
size_t factory_simulation_get_rail_network_count(
    const FactorySimulation *simulation);
const FactoryRailNetworkInspection *factory_simulation_get_rail_network(
    const FactorySimulation *simulation,size_t index);

#endif
