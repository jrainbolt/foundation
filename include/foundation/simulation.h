#ifndef FOUNDATION_SIMULATION_H
#define FOUNDATION_SIMULATION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <foundation/world.h>
#include "foundation/command.h"
#include "foundation/belt.h"
#include "foundation/assembler.h"
#include "foundation/extractor.h"
#include "foundation/refinery.h"
#include "foundation/storage.h"
#include "foundation/splitter.h"

typedef struct FactorySimulation FactorySimulation;

/*
 * Creates a simulation that borrows world. The world must remain alive until
 * factory_simulation_destroy is called.
 */
FactorySimulation *factory_simulation_create(FactoryWorld *world);
void factory_simulation_destroy(FactorySimulation *simulation);

/*
 * Queues a copy of command. OK means queued, not that the gameplay action
 * will succeed. The fixed queue holds FACTORY_COMMAND_QUEUE_CAPACITY entries.
 */
FactoryResult factory_simulation_submit_command(
    FactorySimulation *simulation,
    const FactoryCommand *command
);

/* Applies queued commands FIFO, updates extractors, then increments the tick. */
void factory_simulation_tick(FactorySimulation *simulation);
uint64_t factory_simulation_get_tick(const FactorySimulation *simulation);
size_t factory_simulation_get_pending_command_count(
    const FactorySimulation *simulation
);

/*
 * Results describe commands applied during the most recent tick, in FIFO
 * order. They are replaced on the next tick.
 */
size_t factory_simulation_get_command_result_count(
    const FactorySimulation *simulation
);
const FactoryCommandResult *factory_simulation_get_command_result(
    const FactorySimulation *simulation,
    size_t index
);

bool factory_simulation_entity_is_valid(
    const FactorySimulation *simulation,
    FactoryEntityId id
);
size_t factory_simulation_get_entity_count(
    const FactorySimulation *simulation
);
bool factory_simulation_is_extractor(
    const FactorySimulation *simulation,
    FactoryEntityId id
);

/* Copies read-only extractor state into out_extractor. */
bool factory_simulation_get_extractor(
    const FactorySimulation *simulation,
    FactoryEntityId id,
    FactoryExtractor *out_extractor
);

bool factory_simulation_is_belt(
    const FactorySimulation *simulation,
    FactoryEntityId id
);
bool factory_simulation_get_belt(
    const FactorySimulation *simulation,
    FactoryEntityId id,
    FactoryBelt *out_belt
);
bool factory_simulation_is_storage(
    const FactorySimulation *simulation,
    FactoryEntityId id
);
bool factory_simulation_get_storage(
    const FactorySimulation *simulation,
    FactoryEntityId id,
    FactoryStorage *out_storage
);
bool factory_simulation_is_refinery(
    const FactorySimulation *simulation,
    FactoryEntityId id
);
bool factory_simulation_get_refinery(
    const FactorySimulation *simulation,
    FactoryEntityId id,
    FactoryRefinery *out_refinery
);
bool factory_simulation_is_assembler(
    const FactorySimulation *simulation,
    FactoryEntityId id
);
bool factory_simulation_get_assembler(
    const FactorySimulation *simulation,
    FactoryEntityId id,
    FactoryAssembler *out_assembler
);
bool factory_simulation_is_splitter(
    const FactorySimulation *simulation,
    FactoryEntityId id
);
bool factory_simulation_get_splitter(
    const FactorySimulation *simulation,
    FactoryEntityId id,
    FactorySplitter *out_splitter
);

#endif
