#ifndef FOUNDATION_HEAT_NETWORK_H
#define FOUNDATION_HEAT_NETWORK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "foundation/entity.h"
#include "foundation/fluid.h"
#include "foundation/heat.h"
#include "foundation/world.h"

typedef uint32_t FactoryHeatNetworkId;
typedef uint32_t FactoryHeatPortSlot;

#define FACTORY_HEAT_NETWORK_NONE 0U
#define FACTORY_HEAT_PORT_REACTOR_OUTPUT 1U
#define FACTORY_HEAT_PORT_EXCHANGER_INPUT 2U
#define FACTORY_HEAT_CONNECTION_NORTH (1U << 0U)
#define FACTORY_HEAT_CONNECTION_EAST (1U << 1U)
#define FACTORY_HEAT_CONNECTION_SOUTH (1U << 2U)
#define FACTORY_HEAT_CONNECTION_WEST (1U << 3U)
#define FACTORY_HEAT_CONNECTION_ALL 15U
#define FACTORY_HEAT_TRANSFER_RATE UINT64_C(100)
#define FACTORY_HEAT_EXCHANGER_WATER_CAPACITY 1000U
#define FACTORY_HEAT_EXCHANGER_STEAM_CAPACITY 1000U
#define FACTORY_HEAT_EXCHANGE_RECIPE_WATER_TO_STEAM 1U

typedef struct {
    uint32_t recipe_id;
    FactoryHeatQuantity heat_input;
    FactoryFluidQuantity water_input;
    FactoryFluidQuantity steam_output;
    uint32_t maximum_cycles_per_tick;
} FactoryHeatExchangeRecipe;

typedef struct {
    FactoryEntityId entity_id;
    int32_t x;
    int32_t y;
    uint32_t connection_mask;
    FactoryHeatNetworkId network_id;
    bool connected;
} FactoryHeatConductorInspection;

typedef struct {
    FactoryEntityId owner_entity_id;
    FactoryHeatPortSlot slot;
    FactoryHeatNetworkId network_id;
    bool connected;
} FactoryHeatPortInspection;

typedef struct {
    FactoryHeatNetworkId network_id;
    uint32_t conductor_count;
    uint32_t port_count;
    uint32_t source_count;
    uint32_t consumer_count;
    FactoryHeatQuantity transferred_last_tick;
} FactoryHeatNetworkInspection;

typedef enum {
    FACTORY_HEAT_EXCHANGER_IDLE = 0,
    FACTORY_HEAT_EXCHANGER_WORKING,
    FACTORY_HEAT_EXCHANGER_DISCONNECTED_HEAT,
    FACTORY_HEAT_EXCHANGER_BLOCKED_NO_HEAT,
    FACTORY_HEAT_EXCHANGER_BLOCKED_NO_WATER,
    FACTORY_HEAT_EXCHANGER_BLOCKED_STEAM_FULL
} FactoryHeatExchangerActivity;

typedef struct {
    FactoryEntityId entity_id;
    FactoryHeatNetworkId heat_network_id;
    FactoryFluidNetworkId water_network_id;
    FactoryFluidNetworkId steam_network_id;
    FactoryFluidQuantity stored_water;
    FactoryFluidQuantity water_capacity;
    FactoryFluidQuantity stored_steam;
    FactoryFluidQuantity steam_capacity;
    FactoryHeatQuantity consumed_heat_last_tick;
    FactoryFluidQuantity consumed_water_last_tick;
    FactoryFluidQuantity produced_steam_last_tick;
    FactoryHeatExchangerActivity activity;
} FactoryHeatExchangerInspection;

typedef struct FactorySimulation FactorySimulation;

const FactoryHeatExchangeRecipe *factory_heat_exchange_recipe_get(
    uint32_t recipe_id);
FactoryResult factory_simulation_get_heat_conductor(
    const FactorySimulation *simulation, FactoryEntityId entity_id,
    FactoryHeatConductorInspection *out_conductor);
FactoryResult factory_simulation_get_heat_port(
    const FactorySimulation *simulation, FactoryEntityId owner_entity_id,
    FactoryHeatPortSlot slot, FactoryHeatPortInspection *out_port);
size_t factory_simulation_get_heat_network_count(
    const FactorySimulation *simulation);
FactoryResult factory_simulation_get_heat_network(
    const FactorySimulation *simulation, size_t index,
    FactoryHeatNetworkInspection *out_network);
FactoryResult factory_simulation_get_heat_exchanger(
    const FactorySimulation *simulation, FactoryEntityId entity_id,
    FactoryHeatExchangerInspection *out_exchanger);

#endif
