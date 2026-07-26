#ifndef FOUNDATION_FLUID_INTERNAL_H
#define FOUNDATION_FLUID_INTERNAL_H

#include "foundation/fluid.h"

typedef struct {
    FactoryEntityId owner_entity_id;
    int32_t x;
    int32_t y;
    FactoryFluidClassMask accepted_fluid_classes;
    FactoryFluidType fluid_type;
    FactoryFluidQuantity quantity;
    FactoryFluidQuantity capacity;
} FactoryFluidStorage;

typedef struct {
    FactoryFluidStorage *items;
    size_t count;
    size_t capacity;
} FactoryFluidStorageStore;

typedef struct {
    FactoryEntityId entity_id;
    int32_t x;
    int32_t y;
} FactoryPipe;

typedef struct {
    FactoryPipe *items;
    size_t count;
    size_t capacity;
} FactoryPipeStore;

typedef struct {
    FactoryEntityId owner_entity_id;
    FactoryEntityId storage_owner_entity_id;
    int32_t x;
    int32_t y;
    uint32_t allowed_directions;
    FactoryFluidClassMask accepted_fluid_classes;
} FactoryFluidPort;

typedef struct {
    FactoryFluidPort *items;
    size_t count;
    size_t capacity;
} FactoryFluidPortStore;

typedef struct {
    FactoryPipeInspection *pipes;
    size_t pipe_count;
    FactoryFluidPortInspection *ports;
    size_t port_count;
    FactoryFluidNetworkInspection *networks;
    size_t network_count;
    bool dirty;
} FactoryFluidNetworkState;

void factory_fluid_storage_store_destroy(FactoryFluidStorageStore *store);
bool factory_fluid_storage_store_reserve_one(FactoryFluidStorageStore *store);
void factory_fluid_storage_store_add(
    FactoryFluidStorageStore *store,
    FactoryEntityId owner,
    int32_t x,
    int32_t y,
    FactoryFluidClassMask accepted_classes,
    FactoryFluidQuantity capacity
);
const FactoryFluidStorage *factory_fluid_storage_store_find(
    const FactoryFluidStorageStore *store, FactoryEntityId owner
);
FactoryFluidStorage *factory_fluid_storage_store_find_mutable(
    FactoryFluidStorageStore *store, FactoryEntityId owner
);
bool factory_fluid_storage_store_remove(
    FactoryFluidStorageStore *store, FactoryEntityId owner
);

FactoryResult factory_fluid_storage_insert(
    FactoryFluidStorage *storage,
    FactoryFluidType fluid_type,
    FactoryFluidQuantity quantity
);
FactoryResult factory_fluid_storage_remove(
    FactoryFluidStorage *storage,
    FactoryFluidQuantity quantity,
    FactoryFluidType *out_removed_type
);
FactoryResult factory_fluid_storage_transfer(
    FactoryFluidStorage *source,
    FactoryFluidStorage *destination,
    FactoryFluidQuantity quantity,
    FactoryFluidType *out_transferred_type
);
void factory_pipe_store_destroy(FactoryPipeStore *store);
bool factory_pipe_store_reserve_one(FactoryPipeStore *store);
void factory_pipe_store_add(
    FactoryPipeStore *store, FactoryEntityId id, int32_t x, int32_t y
);
const FactoryPipe *factory_pipe_store_find(
    const FactoryPipeStore *store, FactoryEntityId id
);
bool factory_pipe_store_remove(FactoryPipeStore *store, FactoryEntityId id);
void factory_fluid_port_store_destroy(FactoryFluidPortStore *store);
bool factory_fluid_port_store_reserve_one(FactoryFluidPortStore *store);
void factory_fluid_port_store_add(
    FactoryFluidPortStore *store, FactoryEntityId owner, int32_t x, int32_t y,
    FactoryFluidClassMask accepted_classes
);
bool factory_fluid_port_store_remove(
    FactoryFluidPortStore *store, FactoryEntityId owner
);
void factory_fluid_network_state_destroy(FactoryFluidNetworkState *state);
FactoryResult factory_fluid_network_rebuild(
    FactorySimulation *simulation, bool emit_events
);
void factory_fluid_network_transfer(FactorySimulation *simulation);

#endif
