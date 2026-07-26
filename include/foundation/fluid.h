#ifndef FOUNDATION_FLUID_H
#define FOUNDATION_FLUID_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "foundation/entity.h"
#include "foundation/world.h"

typedef uint32_t FactoryFluidQuantity;
typedef uint32_t FactoryFluidClassMask;

#define FACTORY_FLUID_CLASS_AQUEOUS (1U << 0U)
#define FACTORY_FLUID_TANK_CAPACITY 10000U
#define FACTORY_PIPE_TRANSFER_RATE 100U
#define FACTORY_FLUID_NETWORK_NONE 0U

typedef uint32_t FactoryFluidNetworkId;

typedef enum {
    FACTORY_FLUID_CONNECTION_NORTH = 1U << 0U,
    FACTORY_FLUID_CONNECTION_EAST = 1U << 1U,
    FACTORY_FLUID_CONNECTION_SOUTH = 1U << 2U,
    FACTORY_FLUID_CONNECTION_WEST = 1U << 3U
} FactoryFluidConnection;

#define FACTORY_FLUID_CONNECTION_ALL 15U

typedef enum {
    FACTORY_FLUID_NONE = 0,
    FACTORY_FLUID_WATER
} FactoryFluidType;

typedef struct {
    FactoryFluidType fluid_type;
    const char *display_name;
    FactoryFluidClassMask fluid_class;
} FactoryFluidDefinition;

typedef struct {
    FactoryEntityId owner_entity_id;
    int32_t x;
    int32_t y;
    FactoryFluidClassMask accepted_fluid_classes;
    FactoryFluidType fluid_type;
    FactoryFluidQuantity quantity;
    FactoryFluidQuantity capacity;
    FactoryFluidQuantity free_capacity;
    FactoryFluidNetworkId network_id;
} FactoryFluidStorageInspection;

typedef struct {
    FactoryEntityId entity_id;
    int32_t x;
    int32_t y;
    uint32_t connection_mask;
    FactoryFluidNetworkId network_id;
} FactoryPipeInspection;

typedef struct {
    FactoryEntityId owner_entity_id;
    FactoryEntityId storage_owner_entity_id;
    int32_t x;
    int32_t y;
    uint32_t allowed_directions;
    FactoryFluidClassMask accepted_fluid_classes;
    FactoryFluidNetworkId network_id;
} FactoryFluidPortInspection;

typedef struct {
    FactoryFluidNetworkId network_id;
    uint32_t pipe_count;
    uint32_t port_count;
} FactoryFluidNetworkInspection;

typedef struct FactorySimulation FactorySimulation;

size_t factory_fluid_definition_count(void);
const FactoryFluidDefinition *factory_fluid_definition_at(size_t index);
const FactoryFluidDefinition *factory_fluid_definition_get(
    FactoryFluidType fluid_type
);
bool factory_fluid_definition_is_valid(
    const FactoryFluidDefinition *definition
);
const char *factory_fluid_name(FactoryFluidType fluid_type);

FactoryResult factory_simulation_get_fluid_storage(
    const FactorySimulation *simulation,
    FactoryEntityId owner_entity_id,
    FactoryFluidStorageInspection *out_storage
);
FactoryResult factory_simulation_get_pipe(
    const FactorySimulation *simulation,
    FactoryEntityId entity_id,
    FactoryPipeInspection *out_pipe
);
FactoryResult factory_simulation_get_fluid_port(
    const FactorySimulation *simulation,
    FactoryEntityId owner_entity_id,
    FactoryFluidPortInspection *out_port
);
size_t factory_simulation_get_fluid_network_count(
    const FactorySimulation *simulation
);
FactoryResult factory_simulation_get_fluid_network(
    const FactorySimulation *simulation,
    size_t index,
    FactoryFluidNetworkInspection *out_network
);

/*
 * These operations enqueue deterministic commands. On the next simulation
 * tick they execute in FIFO order and report their authoritative result
 * through the normal command-result API.
 */
FactoryResult factory_simulation_submit_fluid_insert(
    FactorySimulation *simulation,
    FactoryEntityId destination_entity_id,
    FactoryFluidType fluid_type,
    FactoryFluidQuantity quantity
);
FactoryResult factory_simulation_submit_fluid_remove(
    FactorySimulation *simulation,
    FactoryEntityId source_entity_id,
    FactoryFluidQuantity quantity
);
FactoryResult factory_simulation_submit_fluid_transfer(
    FactorySimulation *simulation,
    FactoryEntityId source_entity_id,
    FactoryEntityId destination_entity_id,
    FactoryFluidQuantity quantity
);

#endif
