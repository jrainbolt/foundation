#ifndef FOUNDATION_STEAM_TURBINE_H
#define FOUNDATION_STEAM_TURBINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "foundation/fluid.h"
#include "foundation/power.h"

#define FACTORY_STEAM_TURBINE_STORAGE_CAPACITY 2000U
#define FACTORY_STEAM_TURBINE_MAX_OUTPUT 200U

typedef enum {
    FACTORY_STEAM_TURBINE_DEFINITION_NONE = 0,
    FACTORY_STEAM_TURBINE_DEFINITION_BASIC
} FactorySteamTurbineDefinitionId;

typedef enum {
    FACTORY_STEAM_TURBINE_IDLE = 0,
    FACTORY_STEAM_TURBINE_WORKING,
    FACTORY_STEAM_TURBINE_DISCONNECTED_FLUID,
    FACTORY_STEAM_TURBINE_DISCONNECTED_POWER,
    FACTORY_STEAM_TURBINE_BLOCKED_NO_STEAM,
    FACTORY_STEAM_TURBINE_BLOCKED_INSUFFICIENT_STEAM,
    FACTORY_STEAM_TURBINE_BLOCKED_NO_DEMAND,
    FACTORY_STEAM_TURBINE_BLOCKED_EXHAUST_FULL
} FactorySteamTurbineActivity;

typedef struct {
    FactorySteamTurbineDefinitionId definition_id;
    FactoryFluidType input_fluid;
    FactoryFluidType exhaust_fluid;
    FactoryFluidQuantity steam_per_cycle;
    FactoryFluidQuantity exhaust_per_cycle;
    FactoryPowerUnits energy_per_cycle;
    uint32_t maximum_cycles_per_tick;
    FactoryPowerUnits maximum_output_per_tick;
    FactoryFluidQuantity storage_capacity;
    FactoryFluidQuantity exhaust_capacity;
    uint32_t construction_cost;
} FactorySteamTurbineDefinition;

typedef struct {
    FactoryEntityId entity_id;
    FactorySteamTurbineDefinitionId definition_id;
    FactoryFluidType steam_fluid;
    FactoryFluidQuantity stored_steam;
    FactoryFluidQuantity steam_capacity;
    FactoryFluidType exhaust_fluid;
    FactoryFluidQuantity stored_exhaust;
    FactoryFluidQuantity exhaust_capacity;
    FactoryFluidNetworkId fluid_network_id;
    FactoryFluidNetworkId exhaust_network_id;
    FactoryPowerNetworkId power_network_id;
    bool fluid_connected;
    bool exhaust_connected;
    bool power_connected;
    FactoryFluidQuantity steam_per_cycle;
    FactoryFluidQuantity exhaust_per_cycle;
    FactoryPowerUnits energy_per_cycle;
    FactoryPowerUnits maximum_output_per_tick;
    FactoryPowerUnits available_output;
    FactoryPowerUnits actual_output;
    FactoryFluidQuantity steam_consumed_last_tick;
    FactoryFluidQuantity exhaust_produced_last_tick;
    uint32_t completed_cycles_last_tick;
    FactorySteamTurbineActivity activity;
} FactorySteamTurbineInspection;

size_t factory_steam_turbine_definition_count(void);
const FactorySteamTurbineDefinition *factory_steam_turbine_definition_at(
    size_t index);
const FactorySteamTurbineDefinition *factory_steam_turbine_definition_get(
    FactorySteamTurbineDefinitionId definition_id);
bool factory_steam_turbine_definition_is_valid(
    const FactorySteamTurbineDefinition *definition);

typedef struct FactorySimulation FactorySimulation;
FactoryResult factory_simulation_get_steam_turbine(
    const FactorySimulation *simulation,
    FactoryEntityId entity_id,
    FactorySteamTurbineInspection *out_turbine);

#endif
