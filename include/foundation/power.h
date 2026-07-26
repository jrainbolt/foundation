#ifndef FOUNDATION_POWER_H
#define FOUNDATION_POWER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "foundation/entity.h"
#include "foundation/world.h"

typedef uint32_t FactoryPowerUnits;
typedef uint64_t FactoryPowerTotal;
typedef uint32_t FactoryPowerNetworkId;

#define FACTORY_POWER_NETWORK_NONE 0U
#define FACTORY_POWER_POLE_MACHINE_RADIUS 3U
#define FACTORY_POWER_POLE_WIRE_RADIUS 6U
#define FACTORY_BASIC_GENERATOR_CAPACITY 100U

#define FACTORY_POWER_DEMAND_EXTRACTOR 10U
#define FACTORY_POWER_DEMAND_REFINERY 20U
#define FACTORY_POWER_DEMAND_ASSEMBLER 25U
#define FACTORY_POWER_DEMAND_INSERTER 5U

typedef struct {
    FactoryEntityId entity_id;
    int32_t x;
    int32_t y;
} FactoryPowerPole;

typedef struct {
    FactoryEntityId entity_id;
    int32_t x;
    int32_t y;
    FactoryPowerUnits generation_capacity;
} FactoryPowerGenerator;

typedef struct {
    FactoryEntityId entity_id;
    int32_t x;
    int32_t y;
    uint32_t machine_radius;
    uint32_t wire_radius;
    FactoryPowerNetworkId network_id;
    uint32_t connected_pole_count;
} FactoryPowerPoleInspection;

typedef struct {
    FactoryEntityId entity_id;
    int32_t x;
    int32_t y;
    FactoryPowerUnits generation_capacity;
    FactoryEntityId attached_pole_id;
    FactoryPowerNetworkId network_id;
    bool connected;
    /*
     * The exact output this generator committed to during this tick's
     * preflight allocation. Transient (rebuilt every tick, never
     * snapshotted): factory_power_consume_generation consumes precisely
     * this amount from this generator rather than re-deriving an amount
     * independently.
     */
    FactoryPowerUnits committed_output;
} FactoryPowerGeneratorInspection;

typedef struct {
    FactoryEntityId entity_id;
    FactoryPowerUnits demand;
    FactoryEntityId attached_pole_id;
    FactoryPowerNetworkId network_id;
    bool connected;
    bool powered;
} FactoryPowerConsumerInspection;

typedef struct {
    FactoryPowerNetworkId network_id;
    FactoryPowerTotal total_generation;
    FactoryPowerTotal total_demand;
    FactoryPowerTotal allocated_power;
    FactoryPowerTotal unused_generation;
    FactoryPowerTotal accumulator_charge;
    FactoryPowerTotal accumulator_discharge;
    uint32_t pole_count;
    uint32_t generator_count;
    uint32_t accumulator_count;
    uint32_t consumer_count;
    uint32_t powered_consumer_count;
    uint32_t unpowered_consumer_count;
} FactoryPowerNetworkInspection;

typedef struct {
    FactoryEntityId pole_a;
    FactoryEntityId pole_b;
} FactoryPowerConnectionInspection;

typedef struct FactorySimulation FactorySimulation;

FactoryResult factory_simulation_get_power_pole(
    const FactorySimulation *simulation,
    FactoryEntityId entity_id,
    FactoryPowerPoleInspection *out_pole
);
FactoryResult factory_simulation_get_power_generator(
    const FactorySimulation *simulation,
    FactoryEntityId entity_id,
    FactoryPowerGeneratorInspection *out_generator
);
FactoryResult factory_simulation_get_power_consumer(
    const FactorySimulation *simulation,
    FactoryEntityId entity_id,
    FactoryPowerConsumerInspection *out_consumer
);
size_t factory_simulation_get_power_network_count(
    const FactorySimulation *simulation
);
FactoryResult factory_simulation_get_power_network(
    const FactorySimulation *simulation,
    size_t index,
    FactoryPowerNetworkInspection *out_network
);
size_t factory_simulation_get_power_connection_count(
    const FactorySimulation *simulation
);
FactoryResult factory_simulation_get_power_connection(
    const FactorySimulation *simulation,
    size_t index,
    FactoryPowerConnectionInspection *out_connection
);

#endif
