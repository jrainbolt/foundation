#ifndef FOUNDATION_PRESENTATION_H
#define FOUNDATION_PRESENTATION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "foundation/simulation.h"

#define FACTORY_PRESENTATION_DIRECTION_NONE (-1)
#define FACTORY_PRESENTATION_STORAGE_ITEM_COUNT 8U

typedef enum {
    FACTORY_PRESENTATION_MACHINE_STATUS_NONE = 0,
    FACTORY_PRESENTATION_MACHINE_STATUS_IDLE,
    FACTORY_PRESENTATION_MACHINE_STATUS_WORKING,
    FACTORY_PRESENTATION_MACHINE_STATUS_BLOCKED_INPUT,
    FACTORY_PRESENTATION_MACHINE_STATUS_BLOCKED_OUTPUT,
    FACTORY_PRESENTATION_MACHINE_STATUS_UNPOWERED
} FactoryPresentationMachineStatus;

typedef struct {
    FactoryResourceType resource_type;
    FactoryItemType produced_item;
    uint32_t progress;
    uint32_t duration;
    FactoryItemType output_item;
    uint32_t output_quantity;
    bool can_progress;
} FactoryPresentationExtractor;

typedef struct {
    FactoryItemType item;
    uint32_t quantity;
    uint32_t movement_progress;
    uint32_t movement_duration;
} FactoryPresentationBelt;

typedef struct {
    uint32_t item_quantities[FACTORY_PRESENTATION_STORAGE_ITEM_COUNT];
    uint32_t total_capacity;
    FactoryItemType configured_output_item;
    FactoryItemType output_item;
    uint32_t output_quantity;
} FactoryPresentationStorage;

typedef struct {
    FactoryRecipeId recipe_id;
    FactoryItemType input_item;
    uint32_t input_quantity;
    FactoryItemType output_item;
    uint32_t output_quantity;
    uint32_t progress;
    uint32_t duration;
    bool processing;
} FactoryPresentationRefinery;

typedef struct {
    FactoryAssemblerRecipeId recipe_id;
    FactoryAssemblerInputSlot
        input_slots[FACTORY_ASSEMBLER_MAX_INPUT_TYPES];
    FactoryItemType output_item;
    uint32_t output_quantity;
    uint32_t progress;
    uint32_t duration;
    bool processing;
} FactoryPresentationAssembler;

typedef struct {
    FactoryItemType item;
    uint32_t quantity;
    FactorySplitterOutput next_output;
} FactoryPresentationSplitter;

typedef struct {
    FactoryItemType held_item;
    uint32_t held_quantity;
    FactoryInserterState state;
    uint32_t progress;
    int32_t source_x;
    int32_t source_y;
    int32_t destination_x;
    int32_t destination_y;
} FactoryPresentationInserter;

typedef struct {
    FactoryPowerNetworkId network_id;
    uint32_t machine_radius;
    uint32_t wire_radius;
    uint32_t connected_pole_count;
} FactoryPresentationPowerPole;

typedef struct {
    FactoryItemType inventory_item;
    uint32_t inventory_quantity;
    FactoryItemType current_fuel_item;
    uint32_t remaining_burn_ticks;
    uint32_t total_burn_duration_ticks;
    FactoryEnergy unreleased_fuel_energy;
    FactoryEnergy released_energy;
    bool active;
} FactoryPresentationBurner;

typedef struct {
    FactoryPowerUnits maximum_output_per_tick;
    FactoryEntityId attached_pole_id;
    FactoryPowerNetworkId network_id;
    FactoryPowerTotal network_allocated_power;
    bool connected;
    FactoryPresentationBurner burner;
} FactoryPresentationPowerSource;

typedef struct {
    FactoryFluidType fluid_type;
    FactoryFluidQuantity quantity;
    FactoryFluidQuantity capacity;
    FactoryFluidNetworkId network_id;
} FactoryPresentationFluidStorage;

typedef struct {
    uint32_t connection_mask;
    FactoryFluidNetworkId network_id;
} FactoryPresentationPipe;

typedef struct {
    FactoryFluidQuantity stored_water;
    FactoryFluidQuantity output_capacity;
    uint32_t progress;
    uint32_t duration;
    FactoryFluidNetworkId output_network_id;
} FactoryPresentationWaterExtractor;

typedef struct {
    FactoryFluidQuantity stored_water;
    FactoryFluidQuantity water_capacity;
    FactoryFluidQuantity stored_steam;
    FactoryFluidQuantity steam_capacity;
    FactoryFluidNetworkId input_network_id;
    FactoryFluidNetworkId output_network_id;
    FactoryPresentationBurner burner;
    bool conversion_active;
} FactoryPresentationBoiler;

typedef struct {
    FactoryFluidQuantity stored_steam;
    FactoryFluidQuantity steam_capacity;
    FactoryFluidNetworkId steam_network_id;
    FactoryPowerNetworkId power_network_id;
    FactoryPowerUnits maximum_output_per_tick;
    FactoryPowerUnits available_generation;
    FactoryPowerUnits generated_last_tick;
    bool active;
} FactoryPresentationSteamEngine;

typedef struct {
    FactoryPowerNetworkId power_network_id;
    FactoryPowerUnits maximum_output;
    FactoryPowerUnits available_output;
    FactoryPowerUnits actual_output;
    bool active;
} FactoryPresentationSolarGenerator;

typedef struct {
    FactoryElectricalEnergy stored_energy;
    FactoryElectricalEnergy capacity;
    FactoryPowerUnits maximum_charge_rate;
    FactoryPowerUnits maximum_discharge_rate;
    FactoryPowerUnits charged_last_tick;
    FactoryPowerUnits discharged_last_tick;
    FactoryAccumulatorActivity activity;
    FactoryPowerNetworkId power_network_id;
    bool connected;
} FactoryPresentationAccumulator;

typedef struct {
    FactoryHeatQuantity stored_heat;
    FactoryHeatQuantity heat_capacity;
    FactoryNuclearFuelId inventory_fuel_id;
    uint32_t inventory_quantity;
    FactoryNuclearFuelId active_fuel_id;
    uint32_t remaining_burn_ticks;
    FactoryHeatQuantity remaining_heat_yield;
    FactoryHeatQuantity generated_last_tick;
    FactoryReactorActivity activity;
    FactoryHeatNetworkId heat_network_id;
    bool heat_connected;
} FactoryPresentationReactor;

typedef struct {
    uint32_t connection_mask;
    FactoryHeatNetworkId heat_network_id;
    bool connected;
} FactoryPresentationHeatConductor;

typedef struct {
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
} FactoryPresentationHeatExchanger;

typedef struct {
    FactoryFluidType steam_fluid;
    FactoryFluidQuantity stored_steam;
    FactoryFluidQuantity steam_capacity;
    FactoryFluidNetworkId fluid_network_id;
    FactoryPowerNetworkId power_network_id;
    bool fluid_connected;
    bool power_connected;
    FactorySteamTurbineDefinitionId definition_id;
    FactoryPowerUnits maximum_output;
    FactoryPowerUnits available_output;
    FactoryPowerUnits actual_output;
    FactoryFluidQuantity steam_consumed_last_tick;
    uint32_t completed_cycles_last_tick;
    FactorySteamTurbineActivity activity;
} FactoryPresentationSteamTurbine;

typedef struct {
    FactoryEntityId entity_id;
    FactoryEntityType entity_type;
    int32_t x;
    int32_t y;
    int32_t direction;
    FactoryPresentationMachineStatus status;
    bool powered;
    union {
        FactoryPresentationExtractor extractor;
        FactoryPresentationBelt belt;
        FactoryPresentationStorage storage;
        FactoryPresentationRefinery refinery;
        FactoryPresentationAssembler assembler;
        FactoryPresentationSplitter splitter;
        FactoryPresentationInserter inserter;
        FactoryPresentationPowerPole power_pole;
        FactoryPresentationPowerSource power_source;
        FactoryPresentationFluidStorage fluid_storage;
        FactoryPresentationPipe pipe;
        FactoryPresentationWaterExtractor water_extractor;
        FactoryPresentationBoiler boiler;
        FactoryPresentationSteamEngine steam_engine;
        FactoryPresentationSolarGenerator solar_generator;
        FactoryPresentationAccumulator accumulator;
        FactoryPresentationReactor reactor;
        FactoryPresentationHeatConductor heat_conductor;
        FactoryPresentationHeatExchanger heat_exchanger;
        FactoryPresentationSteamTurbine steam_turbine;
    } data;
} FactoryPresentationEntity;

typedef struct {
    int32_t x;
    int32_t y;
    FactoryResourceType resource_type;
    uint32_t remaining_quantity;
    FactoryEntityId occupying_entity_id;
} FactoryPresentationResource;

typedef struct {
    FactoryEntityId pole_a;
    FactoryEntityId pole_b;
} FactoryPresentationPowerEdge;

typedef struct FactoryPresentationSnapshot FactoryPresentationSnapshot;

FactoryPresentationSnapshot *factory_presentation_snapshot_create(void);
void factory_presentation_snapshot_destroy(
    FactoryPresentationSnapshot *snapshot
);
void factory_presentation_snapshot_clear(
    FactoryPresentationSnapshot *snapshot
);
FactoryResult factory_presentation_snapshot_rebuild(
    FactoryPresentationSnapshot *snapshot,
    const FactorySimulation *simulation
);

uint64_t factory_presentation_snapshot_get_tick(
    const FactoryPresentationSnapshot *snapshot
);
uint64_t factory_presentation_snapshot_get_day(
    const FactoryPresentationSnapshot *snapshot
);
uint32_t factory_presentation_snapshot_get_time_of_day(
    const FactoryPresentationSnapshot *snapshot
);
size_t factory_presentation_snapshot_get_entity_count(
    const FactoryPresentationSnapshot *snapshot
);
const FactoryPresentationEntity *factory_presentation_snapshot_get_entity(
    const FactoryPresentationSnapshot *snapshot, size_t index
);
size_t factory_presentation_snapshot_get_resource_count(
    const FactoryPresentationSnapshot *snapshot
);
const FactoryPresentationResource *factory_presentation_snapshot_get_resource(
    const FactoryPresentationSnapshot *snapshot, size_t index
);
size_t factory_presentation_snapshot_get_power_edge_count(
    const FactoryPresentationSnapshot *snapshot
);
const FactoryPresentationPowerEdge *
factory_presentation_snapshot_get_power_edge(
    const FactoryPresentationSnapshot *snapshot, size_t index
);

#endif
