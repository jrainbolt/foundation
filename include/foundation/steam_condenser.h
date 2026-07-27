#ifndef FOUNDATION_STEAM_CONDENSER_H
#define FOUNDATION_STEAM_CONDENSER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "foundation/fluid.h"
#include "foundation/power.h"

#define FACTORY_STEAM_CONDENSER_STEAM_CAPACITY 2000U
#define FACTORY_STEAM_CONDENSER_WATER_CAPACITY 2000U

typedef enum {
    FACTORY_STEAM_CONDENSER_DEFINITION_NONE = 0,
    FACTORY_STEAM_CONDENSER_DEFINITION_BASIC
} FactorySteamCondenserDefinitionId;

typedef enum {
    FACTORY_STEAM_CONDENSER_IDLE = 0,
    FACTORY_STEAM_CONDENSER_WORKING,
    FACTORY_STEAM_CONDENSER_NO_POWER,
    FACTORY_STEAM_CONDENSER_NO_STEAM,
    FACTORY_STEAM_CONDENSER_OUTPUT_FULL,
    FACTORY_STEAM_CONDENSER_DISCONNECTED_FLUID,
    FACTORY_STEAM_CONDENSER_DISCONNECTED_POWER
} FactorySteamCondenserActivity;

typedef struct {
    FactorySteamCondenserDefinitionId definition_id;
    FactoryFluidType input_fluid;
    FactoryFluidType output_fluid;
    FactoryFluidQuantity steam_per_cycle;
    FactoryFluidQuantity water_per_cycle;
    FactoryPowerUnits power_per_cycle;
    uint32_t maximum_cycles_per_tick;
    FactoryFluidQuantity steam_capacity;
    FactoryFluidQuantity water_capacity;
    uint32_t construction_cost;
} FactorySteamCondenserDefinition;

typedef struct {
    FactoryEntityId entity_id;
    FactorySteamCondenserDefinitionId definition_id;
    FactoryFluidType steam_fluid;
    FactoryFluidQuantity stored_steam;
    FactoryFluidQuantity steam_capacity;
    FactoryFluidType water_fluid;
    FactoryFluidQuantity stored_water;
    FactoryFluidQuantity water_capacity;
    FactoryFluidNetworkId steam_network_id;
    FactoryFluidNetworkId water_network_id;
    FactoryPowerNetworkId power_network_id;
    bool fluid_connected;
    bool power_connected;
    bool powered;
    FactoryPowerUnits power_per_cycle;
    FactoryFluidQuantity steam_consumed_last_tick;
    FactoryFluidQuantity water_produced_last_tick;
    uint32_t completed_cycles_last_tick;
    FactorySteamCondenserActivity activity;
} FactorySteamCondenserInspection;

size_t factory_steam_condenser_definition_count(void);
const FactorySteamCondenserDefinition *factory_steam_condenser_definition_at(
    size_t index);
const FactorySteamCondenserDefinition *factory_steam_condenser_definition_get(
    FactorySteamCondenserDefinitionId definition_id);
bool factory_steam_condenser_definition_is_valid(
    const FactorySteamCondenserDefinition *definition);

typedef struct FactorySimulation FactorySimulation;
FactoryResult factory_simulation_get_steam_condenser(
    const FactorySimulation *simulation,
    FactoryEntityId entity_id,
    FactorySteamCondenserInspection *out_condenser);

#endif
