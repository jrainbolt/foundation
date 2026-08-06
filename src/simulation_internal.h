#ifndef FOUNDATION_SIMULATION_INTERNAL_H
#define FOUNDATION_SIMULATION_INTERNAL_H

#include "foundation/simulation.h"

#include "assembler_internal.h"
#include "belt_internal.h"
#include "extractor_internal.h"
#include "inserter_internal.h"
#include "refinery_internal.h"
#include "splitter_internal.h"
#include "storage_internal.h"
#include "construction_inventory_internal.h"
#include "entity_internal.h"
#include "world_internal.h"
#include "power_internal.h"
#include "event_internal.h"
#include "burner_internal.h"
#include "fluid_internal.h"
#include "fluid_machine_internal.h"
#include "steam_engine_internal.h"
#include "steam_turbine_internal.h"
#include "steam_condenser_internal.h"
#include "clock_internal.h"
#include "solar_internal.h"
#include "accumulator_internal.h"
#include "reactor_internal.h"
#include "heat_network_internal.h"
#include "tick_preflight_internal.h"

struct FactorySimulation {
    FactorySimulationClock clock;
    FactoryWorld *world;
    bool owns_world;
    FactoryEntityManager *entities;
    FactoryExtractorStore extractors;
    FactoryRefineryStore refineries;
    FactoryAssemblerStore assemblers;
    FactorySplitterStore splitters;
    FactoryInserterStore inserters;
    FactoryBeltStore belts;
    FactoryStorageStore storages;
    FactoryPowerPoleStore power_poles;
    FactoryPowerGeneratorStore power_generators;
    FactoryBurnerStore burners;
    FactoryFluidStorageStore fluid_storages;
    FactoryPipeStore pipes;
    FactoryFluidPortStore fluid_ports;
    FactoryFluidNetworkState fluid_networks;
    FactoryWaterExtractorStore water_extractors;
    FactoryBoilerStore boilers;
    FactorySteamEngineStore steam_engines;
    FactorySteamTurbineStore steam_turbines;
    FactorySteamCondenserStore steam_condensers;
    FactorySolarGeneratorStore solar_generators;
    FactoryAccumulatorStore accumulators;
    FactoryReactorStore reactors;
    FactoryHeatConductorStore heat_conductors;
    FactoryHeatPortStore heat_ports;
    FactoryHeatExchangerStore heat_exchangers;
    FactoryHeatNetworkState heat_networks;
    FactoryTickPreflight tick_preflight;
    /* Test-fixture setup only; production simulations leave this zero. */
    uint32_t fixture_initial_generator_fuel;
    FactoryPowerState power;
    FactoryEventBatch events;
    FactoryConstructionInventory construction_inventory;
    FactoryCommand commands[FACTORY_COMMAND_QUEUE_CAPACITY];
    size_t command_count;
    FactoryCommandResult results[FACTORY_COMMAND_QUEUE_CAPACITY];
    size_t result_count;
};

#endif
