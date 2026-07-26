#ifndef FOUNDATION_POWER_INTERNAL_H
#define FOUNDATION_POWER_INTERNAL_H

#include "foundation/power.h"

typedef struct {
    FactoryPowerPole *items;
    size_t count;
    size_t capacity;
} FactoryPowerPoleStore;

typedef struct {
    FactoryPowerGenerator *items;
    size_t count;
    size_t capacity;
} FactoryPowerGeneratorStore;

typedef struct {
    FactoryEntityId entity_id;
    FactoryEntityId attached_pole_id;
    FactoryPowerNetworkId network_id;
    bool connected;
} FactoryPowerAccumulatorInspection;

typedef struct {
    FactoryPowerPoleInspection *poles;
    size_t pole_count;
    FactoryPowerGeneratorInspection *generators;
    size_t generator_count;
    FactoryPowerAccumulatorInspection *accumulators;
    size_t accumulator_count;
    FactoryPowerConsumerInspection *consumers;
    size_t consumer_count;
    FactoryPowerConnectionInspection *connections;
    size_t connection_count;
    FactoryPowerNetworkInspection *networks;
    size_t network_count;
} FactoryPowerState;

void factory_power_pole_store_destroy(FactoryPowerPoleStore *store);
void factory_power_generator_store_destroy(FactoryPowerGeneratorStore *store);
bool factory_power_pole_store_reserve_one(FactoryPowerPoleStore *store);
bool factory_power_generator_store_reserve_one(FactoryPowerGeneratorStore *store);
void factory_power_pole_store_add(
    FactoryPowerPoleStore *store, FactoryEntityId id, int32_t x, int32_t y
);
void factory_power_generator_store_add(
    FactoryPowerGeneratorStore *store,
    FactoryEntityId id,
    int32_t x,
    int32_t y
);
const FactoryPowerPole *factory_power_pole_store_find(
    const FactoryPowerPoleStore *store, FactoryEntityId id
);
const FactoryPowerGenerator *factory_power_generator_store_find(
    const FactoryPowerGeneratorStore *store, FactoryEntityId id
);
bool factory_power_pole_store_remove(
    FactoryPowerPoleStore *store, FactoryEntityId id
);
bool factory_power_generator_store_remove(
    FactoryPowerGeneratorStore *store, FactoryEntityId id
);
void factory_power_state_destroy(FactoryPowerState *state);
FactoryPowerUnits factory_power_source_available_generation(
    const FactorySimulation *simulation, FactoryEntityId entity_id
);
FactoryResult factory_power_rebuild(
    FactorySimulation *simulation, bool emit_transitions
);
void factory_power_consume_generation(FactorySimulation *simulation);

/*
 * The reusable quantum-aware allocation step: attempts to cover one
 * consumer's demand from the network's generators, in ascending entity-ID
 * order, and commits per-generator output only if the complete demand is
 * met. `available`, `quantum` (0 = continuous; otherwise the generator's
 * indivisible output unit), and `unlocked` are parallel arrays indexed the
 * same as `generators`; `generators[i].committed_output` and `unlocked[i]`
 * are the only state mutated, and only on success. `committed_delta` is
 * caller-owned scratch space sized `generator_count`, reused per call.
 * Exposed (not static) so it is independently testable without a full
 * FactorySimulation/entity graph.
 */
bool factory_power_allocate_consumer(
    FactoryPowerGeneratorInspection *generators,
    const FactoryPowerUnits *available,
    const FactoryPowerUnits *quantum,
    bool *unlocked,
    size_t generator_count,
    FactoryPowerNetworkId network_id,
    FactoryPowerUnits demand,
    FactoryPowerUnits *committed_delta
);
bool factory_power_is_entity_powered(
    const FactorySimulation *simulation, FactoryEntityId entity_id
);

#endif
