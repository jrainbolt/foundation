#include "fluid_internal.h"

#include "foundation/command.h"
#include "simulation_internal.h"

#include <stdlib.h>
#include <string.h>

static const FactoryFluidDefinition fluid_definitions[] = {
    {FACTORY_FLUID_WATER, "water", FACTORY_FLUID_CLASS_AQUEOUS},
    {FACTORY_FLUID_STEAM, "steam", FACTORY_FLUID_CLASS_VAPOR}
};

size_t factory_fluid_definition_count(void)
{
    return sizeof(fluid_definitions) / sizeof(fluid_definitions[0]);
}

bool factory_fluid_definition_is_valid(
    const FactoryFluidDefinition *definition
)
{
    return definition != NULL
        && definition->fluid_type > FACTORY_FLUID_NONE
        && definition->fluid_type <= FACTORY_FLUID_STEAM
        && definition->display_name != NULL
        && definition->display_name[0] != '\0'
        && definition->fluid_class != 0U;
}

const FactoryFluidDefinition *factory_fluid_definition_at(size_t index)
{
    return index < factory_fluid_definition_count()
        ? &fluid_definitions[index] : NULL;
}

const FactoryFluidDefinition *factory_fluid_definition_get(
    FactoryFluidType fluid_type
)
{
    size_t index;
    for (index = 0U; index < factory_fluid_definition_count(); ++index)
        if (fluid_definitions[index].fluid_type == fluid_type
            && factory_fluid_definition_is_valid(&fluid_definitions[index]))
            return &fluid_definitions[index];
    return NULL;
}

const char *factory_fluid_name(FactoryFluidType fluid_type)
{
    const FactoryFluidDefinition *definition =
        factory_fluid_definition_get(fluid_type);
    return definition == NULL ? "invalid fluid" : definition->display_name;
}

void factory_fluid_storage_store_destroy(FactoryFluidStorageStore *store)
{
    if (store == NULL) return;
    free(store->items);
    *store = (FactoryFluidStorageStore){0};
}

bool factory_fluid_storage_store_reserve_one(FactoryFluidStorageStore *store)
{
    FactoryFluidStorage *items;
    size_t capacity;
    if (store == NULL) return false;
    if (store->count < store->capacity) return true;
    capacity = store->capacity == 0U ? 4U : store->capacity * 2U;
    if (capacity < store->capacity
        || capacity > SIZE_MAX / sizeof(*items)) return false;
    items = realloc(store->items, capacity * sizeof(*items));
    if (items == NULL) return false;
    store->items = items;
    store->capacity = capacity;
    return true;
}

void factory_fluid_storage_store_add(
    FactoryFluidStorageStore *store,
    FactoryEntityId owner,
    FactoryFluidStorageSlot slot,
    int32_t x,
    int32_t y,
    FactoryFluidClassMask accepted_classes,
    FactoryFluidQuantity capacity
)
{
    store->items[store->count++] = (FactoryFluidStorage){
        owner, slot, x, y, accepted_classes, FACTORY_FLUID_NONE, 0U, capacity
    };
}

const FactoryFluidStorage *factory_fluid_storage_store_find(
    const FactoryFluidStorageStore *store, FactoryEntityId owner
)
{
    size_t index;
    const FactoryFluidStorage *only = NULL;
    if (store == NULL || owner == 0U) return NULL;
    for (index = 0U; index < store->count; ++index)
        if (store->items[index].owner_entity_id == owner) {
            if (store->items[index].slot == FACTORY_FLUID_STORAGE_DEFAULT)
                return &store->items[index];
            if (only != NULL) return NULL;
            only = &store->items[index];
        }
    return only;
}

const FactoryFluidStorage *factory_fluid_storage_store_find_slot(
    const FactoryFluidStorageStore *store, FactoryEntityId owner,
    FactoryFluidStorageSlot slot
)
{
    if (store == NULL || owner == 0U) return NULL;
    for (size_t i = 0U; i < store->count; ++i)
        if (store->items[i].owner_entity_id == owner
            && store->items[i].slot == slot) return &store->items[i];
    return NULL;
}

FactoryFluidStorage *factory_fluid_storage_store_find_slot_mutable(
    FactoryFluidStorageStore *store, FactoryEntityId owner,
    FactoryFluidStorageSlot slot
)
{
    return (FactoryFluidStorage *)factory_fluid_storage_store_find_slot(
        store, owner, slot);
}

FactoryFluidStorage *factory_fluid_storage_store_find_mutable(
    FactoryFluidStorageStore *store, FactoryEntityId owner
)
{
    return (FactoryFluidStorage *)factory_fluid_storage_store_find(
        store, owner);
}

bool factory_fluid_storage_store_remove(
    FactoryFluidStorageStore *store, FactoryEntityId owner
)
{
    size_t index;
    if (store == NULL) return false;
    for (index = 0U; index < store->count; ++index) {
        if (store->items[index].owner_entity_id == owner) {
            --store->count;
            store->items[index] = store->items[store->count];
            return true;
        }
    }
    return false;
}

static bool accepts(
    const FactoryFluidStorage *storage,
    const FactoryFluidDefinition *definition
)
{
    return storage != NULL && definition != NULL
        && (storage->accepted_fluid_classes
            & definition->fluid_class) != 0U;
}

FactoryResult factory_fluid_storage_insert(
    FactoryFluidStorage *storage,
    FactoryFluidType fluid_type,
    FactoryFluidQuantity quantity
)
{
    const FactoryFluidDefinition *definition =
        factory_fluid_definition_get(fluid_type);
    if (storage == NULL || quantity == 0U || definition == NULL)
        return FACTORY_RESULT_INVALID_ARGUMENT;
    if (!accepts(storage, definition))
        return FACTORY_RESULT_FLUID_INCOMPATIBLE;
    if (storage->quantity != 0U && storage->fluid_type != fluid_type)
        return FACTORY_RESULT_FLUID_MISMATCH;
    if (quantity > storage->capacity - storage->quantity)
        return FACTORY_RESULT_FLUID_CAPACITY_EXCEEDED;
    storage->fluid_type = fluid_type;
    storage->quantity += quantity;
    return FACTORY_RESULT_OK;
}

FactoryResult factory_fluid_storage_remove(
    FactoryFluidStorage *storage,
    FactoryFluidQuantity quantity,
    FactoryFluidType *out_removed_type
)
{
    FactoryFluidType removed;
    if (storage == NULL || quantity == 0U || out_removed_type == NULL)
        return FACTORY_RESULT_INVALID_ARGUMENT;
    if (quantity > storage->quantity)
        return FACTORY_RESULT_INSUFFICIENT_FLUID;
    removed = storage->fluid_type;
    storage->quantity -= quantity;
    if (storage->quantity == 0U) storage->fluid_type = FACTORY_FLUID_NONE;
    *out_removed_type = removed;
    return FACTORY_RESULT_OK;
}

FactoryResult factory_fluid_storage_transfer(
    FactoryFluidStorage *source,
    FactoryFluidStorage *destination,
    FactoryFluidQuantity quantity,
    FactoryFluidType *out_transferred_type
)
{
    FactoryFluidType fluid_type;
    const FactoryFluidDefinition *definition;
    if (source == NULL || destination == NULL
        || source == destination || quantity == 0U
        || out_transferred_type == NULL)
        return FACTORY_RESULT_INVALID_ARGUMENT;
    if (quantity > source->quantity)
        return FACTORY_RESULT_INSUFFICIENT_FLUID;
    fluid_type = source->fluid_type;
    definition = factory_fluid_definition_get(fluid_type);
    if (!accepts(destination, definition))
        return FACTORY_RESULT_FLUID_INCOMPATIBLE;
    if (destination->quantity != 0U
        && destination->fluid_type != fluid_type)
        return FACTORY_RESULT_FLUID_MISMATCH;
    if (quantity > destination->capacity - destination->quantity)
        return FACTORY_RESULT_FLUID_CAPACITY_EXCEEDED;
    source->quantity -= quantity;
    if (source->quantity == 0U) source->fluid_type = FACTORY_FLUID_NONE;
    destination->fluid_type = fluid_type;
    destination->quantity += quantity;
    *out_transferred_type = fluid_type;
    return FACTORY_RESULT_OK;
}

FactoryResult factory_simulation_get_fluid_storage(
    const FactorySimulation *simulation,
    FactoryEntityId owner_entity_id,
    FactoryFluidStorageInspection *out_storage
)
{
    if (simulation == NULL || owner_entity_id == 0U || out_storage == NULL)
        return FACTORY_RESULT_INVALID_ARGUMENT;
    return factory_simulation_get_fluid_storage_slot(
        simulation, owner_entity_id, FACTORY_FLUID_STORAGE_DEFAULT,
        out_storage);
}

FactoryResult factory_simulation_get_fluid_storage_slot(
    const FactorySimulation *simulation, FactoryEntityId owner_entity_id,
    FactoryFluidStorageSlot slot, FactoryFluidStorageInspection *out_storage
)
{
    const FactoryFluidStorage *storage;
    if (simulation == NULL || owner_entity_id == 0U || out_storage == NULL)
        return FACTORY_RESULT_INVALID_ARGUMENT;
    storage = factory_fluid_storage_store_find_slot(
        &simulation->fluid_storages, owner_entity_id, slot);
    if (storage == NULL) return FACTORY_RESULT_ENTITY_NOT_FOUND;
    *out_storage = (FactoryFluidStorageInspection){
        storage->owner_entity_id, storage->slot, storage->x, storage->y,
        storage->accepted_fluid_classes, storage->fluid_type,
        storage->quantity, storage->capacity,
        storage->capacity - storage->quantity,
        FACTORY_FLUID_NETWORK_NONE
    };
    for (size_t i = 0U; i < simulation->fluid_networks.port_count; ++i)
        if (simulation->fluid_networks.ports[i].owner_entity_id
                == owner_entity_id
            && simulation->fluid_networks.ports[i].storage_slot == slot) {
            out_storage->network_id =
                simulation->fluid_networks.ports[i].network_id;
            break;
        }
    return FACTORY_RESULT_OK;
}

static bool reserve_array(
    void **items, size_t *capacity, size_t count, size_t width
)
{
    size_t next;
    void *resized;
    if (count < *capacity) return true;
    next = *capacity == 0U ? 4U : *capacity * 2U;
    if (next < *capacity || next > SIZE_MAX / width) return false;
    resized = realloc(*items, next * width);
    if (resized == NULL) return false;
    *items = resized; *capacity = next;
    return true;
}

void factory_pipe_store_destroy(FactoryPipeStore *store)
{
    if (store == NULL) return;
    free(store->items); *store = (FactoryPipeStore){0};
}

bool factory_pipe_store_reserve_one(FactoryPipeStore *store)
{
    return store != NULL && reserve_array(
        (void **)&store->items, &store->capacity, store->count,
        sizeof(*store->items));
}

void factory_pipe_store_add(
    FactoryPipeStore *store, FactoryEntityId id, int32_t x, int32_t y
)
{
    store->items[store->count++] = (FactoryPipe){id, x, y};
}

const FactoryPipe *factory_pipe_store_find(
    const FactoryPipeStore *store, FactoryEntityId id
)
{
    if (store == NULL) return NULL;
    for (size_t i = 0U; i < store->count; ++i)
        if (store->items[i].entity_id == id) return &store->items[i];
    return NULL;
}

bool factory_pipe_store_remove(FactoryPipeStore *store, FactoryEntityId id)
{
    if (store == NULL) return false;
    for (size_t i = 0U; i < store->count; ++i)
        if (store->items[i].entity_id == id) {
            --store->count; store->items[i] = store->items[store->count];
            return true;
        }
    return false;
}

void factory_fluid_port_store_destroy(FactoryFluidPortStore *store)
{
    if (store == NULL) return;
    free(store->items); *store = (FactoryFluidPortStore){0};
}

bool factory_fluid_port_store_reserve_one(FactoryFluidPortStore *store)
{
    return store != NULL && reserve_array(
        (void **)&store->items, &store->capacity, store->count,
        sizeof(*store->items));
}

void factory_fluid_port_store_add(
    FactoryFluidPortStore *store, FactoryEntityId owner,
    FactoryFluidStorageSlot storage_slot, int32_t x, int32_t y,
    uint32_t allowed_directions, FactoryFluidClassMask accepted_classes
)
{
    store->items[store->count++] = (FactoryFluidPort){
        owner, owner, storage_slot, x, y, allowed_directions, accepted_classes
    };
}

bool factory_fluid_port_store_remove(
    FactoryFluidPortStore *store, FactoryEntityId owner
)
{
    if (store == NULL) return false;
    for (size_t i = 0U; i < store->count; ++i)
        if (store->items[i].owner_entity_id == owner) {
            --store->count; store->items[i] = store->items[store->count];
            return true;
        }
    return false;
}

void factory_fluid_network_state_destroy(FactoryFluidNetworkState *state)
{
    if (state == NULL) return;
    free(state->pipes); free(state->ports); free(state->networks);
    *state = (FactoryFluidNetworkState){0};
}

static bool adjacent(int32_t ax, int32_t ay, int32_t bx, int32_t by)
{
    int64_t dx = (int64_t)ax - bx, dy = (int64_t)ay - by;
    return (dx == 0 && (dy == 1 || dy == -1))
        || (dy == 0 && (dx == 1 || dx == -1));
}

static uint32_t direction_to(int32_t ax, int32_t ay, int32_t bx, int32_t by)
{
    if (bx == ax && by == ay - 1) return FACTORY_FLUID_CONNECTION_NORTH;
    if (bx == ax + 1 && by == ay) return FACTORY_FLUID_CONNECTION_EAST;
    if (bx == ax && by == ay + 1) return FACTORY_FLUID_CONNECTION_SOUTH;
    return FACTORY_FLUID_CONNECTION_WEST;
}

static int compare_pipe(const void *a, const void *b)
{
    const FactoryPipeInspection *x = a, *y = b;
    return x->entity_id < y->entity_id ? -1 : x->entity_id > y->entity_id;
}
static int compare_port(const void *a, const void *b)
{
    const FactoryFluidPortInspection *x = a, *y = b;
    if (x->owner_entity_id != y->owner_entity_id)
        return x->owner_entity_id < y->owner_entity_id ? -1 : 1;
    return x->storage_slot < y->storage_slot
        ? -1 : x->storage_slot > y->storage_slot;
}

FactoryResult factory_fluid_network_rebuild(
    FactorySimulation *simulation, bool emit_events
)
{
    FactoryFluidNetworkState next = {0};
    size_t old_network_count;
    size_t old_connection_count = 0U;
    size_t new_connection_count = 0U;
    if (simulation == NULL) return FACTORY_RESULT_INVALID_ARGUMENT;
    if (!simulation->fluid_networks.dirty) return FACTORY_RESULT_OK;
    old_network_count = simulation->fluid_networks.network_count;
    for (size_t i = 0U; i < simulation->fluid_networks.pipe_count; ++i) {
        uint32_t mask = simulation->fluid_networks.pipes[i].connection_mask;
        old_connection_count += ((mask & FACTORY_FLUID_CONNECTION_EAST) != 0U)
            + ((mask & FACTORY_FLUID_CONNECTION_SOUTH) != 0U);
    }
    next.pipe_count = simulation->pipes.count;
    next.port_count = simulation->fluid_ports.count;
    if ((next.pipe_count && (next.pipes = calloc(
            next.pipe_count, sizeof(*next.pipes))) == NULL)
        || (next.port_count && (next.ports = calloc(
            next.port_count, sizeof(*next.ports))) == NULL)
        || (next.pipe_count && (next.networks = calloc(
            next.pipe_count, sizeof(*next.networks))) == NULL)) {
        factory_fluid_network_state_destroy(&next);
        return FACTORY_RESULT_OUT_OF_MEMORY;
    }
    for (size_t i = 0U; i < next.pipe_count; ++i) {
        FactoryPipe p = simulation->pipes.items[i];
        next.pipes[i] = (FactoryPipeInspection){
            p.entity_id, p.x, p.y, 0U, p.entity_id};
    }
    if (next.pipe_count > 1U)
        qsort(next.pipes, next.pipe_count, sizeof(*next.pipes), compare_pipe);
    for (size_t i = 0U; i < next.pipe_count; ++i)
        for (size_t j = i + 1U; j < next.pipe_count; ++j)
            if (adjacent(next.pipes[i].x, next.pipes[i].y,
                    next.pipes[j].x, next.pipes[j].y)) {
                next.pipes[i].connection_mask |= direction_to(
                    next.pipes[i].x, next.pipes[i].y,
                    next.pipes[j].x, next.pipes[j].y);
                next.pipes[j].connection_mask |= direction_to(
                    next.pipes[j].x, next.pipes[j].y,
                    next.pipes[i].x, next.pipes[i].y);
            }
    bool changed;
    do {
        changed = false;
        for (size_t i = 0U; i < next.pipe_count; ++i)
            for (size_t j = i + 1U; j < next.pipe_count; ++j)
                if (adjacent(next.pipes[i].x, next.pipes[i].y,
                        next.pipes[j].x, next.pipes[j].y)) {
                    FactoryFluidNetworkId minimum =
                        next.pipes[i].network_id < next.pipes[j].network_id
                        ? next.pipes[i].network_id : next.pipes[j].network_id;
                    if (next.pipes[i].network_id != minimum
                        || next.pipes[j].network_id != minimum) {
                        next.pipes[i].network_id = minimum;
                        next.pipes[j].network_id = minimum; changed = true;
                    }
                }
    } while (changed);
    for (size_t i = 0U; i < next.port_count; ++i) {
        FactoryFluidPort p = simulation->fluid_ports.items[i];
        next.ports[i] = (FactoryFluidPortInspection){
            p.owner_entity_id, p.storage_owner_entity_id, p.storage_slot,
            p.x, p.y,
            p.allowed_directions, p.accepted_fluid_classes,
            FACTORY_FLUID_NETWORK_NONE};
        for (size_t j = 0U; j < next.pipe_count; ++j)
            if (adjacent(p.x, p.y, next.pipes[j].x, next.pipes[j].y)
                && (p.allowed_directions
                    & direction_to(p.x, p.y,
                        next.pipes[j].x, next.pipes[j].y)) != 0U) {
                FactoryFluidNetworkId id = next.pipes[j].network_id;
                if (next.ports[i].network_id == 0U
                    || id < next.ports[i].network_id)
                    next.ports[i].network_id = id;
                next.pipes[j].connection_mask |= direction_to(
                    next.pipes[j].x, next.pipes[j].y, p.x, p.y);
            }
    }
    if (next.port_count > 1U)
        qsort(next.ports, next.port_count, sizeof(*next.ports), compare_port);
    for (size_t i = 0U; i < next.pipe_count; ++i) {
        FactoryFluidNetworkId id = next.pipes[i].network_id;
        size_t n;
        for (n = 0U; n < next.network_count; ++n)
            if (next.networks[n].network_id == id) break;
        if (n == next.network_count)
            next.networks[next.network_count++] =
                (FactoryFluidNetworkInspection){id, 0U, 0U};
        ++next.networks[n].pipe_count;
    }
    for (size_t i = 0U; i < next.port_count; ++i)
        for (size_t n = 0U; n < next.network_count; ++n)
            if (next.networks[n].network_id == next.ports[i].network_id)
                ++next.networks[n].port_count;
    for (size_t i = 0U; i < next.pipe_count; ++i) {
        uint32_t mask = next.pipes[i].connection_mask;
        new_connection_count += ((mask & FACTORY_FLUID_CONNECTION_EAST) != 0U)
            + ((mask & FACTORY_FLUID_CONNECTION_SOUTH) != 0U);
    }
    if (emit_events) {
        if (old_network_count == 0U && next.network_count != 0U)
            factory_simulation_emit_event(simulation, (FactoryEvent){
                .type = FACTORY_EVENT_FLUID_NETWORK_CREATED,
                .entity_id = next.networks[0].network_id});
        else if (next.network_count < old_network_count)
            factory_simulation_emit_event(simulation, (FactoryEvent){
                .type = FACTORY_EVENT_FLUID_NETWORK_MERGED,
                .entity_id = next.network_count == 0U ? 0U
                    : next.networks[0].network_id});
        else if (next.network_count > old_network_count
            && old_network_count != 0U)
            factory_simulation_emit_event(simulation, (FactoryEvent){
                .type = FACTORY_EVENT_FLUID_NETWORK_SPLIT,
                .entity_id = next.networks[0].network_id});
        if (new_connection_count > old_connection_count)
            factory_simulation_emit_event(simulation, (FactoryEvent){
                .type = FACTORY_EVENT_PIPE_CONNECTED,
                .quantity = (uint32_t)(
                    new_connection_count - old_connection_count)});
        else if (old_connection_count > new_connection_count)
            factory_simulation_emit_event(simulation, (FactoryEvent){
                .type = FACTORY_EVENT_PIPE_DISCONNECTED,
                .quantity = (uint32_t)(
                    old_connection_count - new_connection_count)});
    }
    next.dirty = false;
    factory_fluid_network_state_destroy(&simulation->fluid_networks);
    simulation->fluid_networks = next;
    return FACTORY_RESULT_OK;
}

void factory_fluid_network_transfer(FactorySimulation *simulation)
{
    if (simulation == NULL) return;
    for (size_t n = 0U; n < simulation->fluid_networks.network_count; ++n)
        for (size_t i = 0U; i < simulation->fluid_networks.port_count; ++i)
            for (size_t j = i + 1U;
                j < simulation->fluid_networks.port_count; ++j) {
                FactoryFluidPortInspection *a =
                    &simulation->fluid_networks.ports[i];
                FactoryFluidPortInspection *b =
                    &simulation->fluid_networks.ports[j];
                FactoryFluidStorage *source, *destination;
                FactoryFluidType ignored;
                if (a->network_id !=
                        simulation->fluid_networks.networks[n].network_id
                    || b->network_id != a->network_id) continue;
                source = factory_fluid_storage_store_find_slot_mutable(
                    &simulation->fluid_storages,
                    a->storage_owner_entity_id,
                    a->storage_slot);
                destination = factory_fluid_storage_store_find_slot_mutable(
                    &simulation->fluid_storages,
                    b->storage_owner_entity_id,
                    b->storage_slot);
                if (source == NULL || destination == NULL) continue;
                if (source->quantity < destination->quantity) {
                    FactoryFluidStorage *swap = source;
                    source = destination; destination = swap;
                }
                if (source->quantity == destination->quantity
                    || destination->quantity >= destination->capacity)
                    continue;
                FactoryFluidQuantity amount =
                    (source->quantity - destination->quantity) / 2U;
                if (amount == 0U) continue;
                if (amount > FACTORY_PIPE_TRANSFER_RATE)
                    amount = FACTORY_PIPE_TRANSFER_RATE;
                FactoryFluidQuantity free =
                    destination->capacity - destination->quantity;
                if (amount > free) amount = free;
                (void)factory_fluid_storage_transfer(
                    source, destination, amount, &ignored);
            }
}

FactoryResult factory_simulation_get_pipe(
    const FactorySimulation *simulation, FactoryEntityId id,
    FactoryPipeInspection *out_pipe
)
{
    if (simulation == NULL || id == 0U || out_pipe == NULL)
        return FACTORY_RESULT_INVALID_ARGUMENT;
    for (size_t i = 0U; i < simulation->fluid_networks.pipe_count; ++i)
        if (simulation->fluid_networks.pipes[i].entity_id == id) {
            *out_pipe = simulation->fluid_networks.pipes[i];
            return FACTORY_RESULT_OK;
        }
    return FACTORY_RESULT_ENTITY_NOT_FOUND;
}

FactoryResult factory_simulation_get_fluid_port(
    const FactorySimulation *simulation, FactoryEntityId id,
    FactoryFluidPortInspection *out_port
)
{
    if (simulation == NULL || id == 0U || out_port == NULL)
        return FACTORY_RESULT_INVALID_ARGUMENT;
    for (size_t i = 0U; i < simulation->fluid_networks.port_count; ++i)
        if (simulation->fluid_networks.ports[i].owner_entity_id == id) {
            *out_port = simulation->fluid_networks.ports[i];
            return FACTORY_RESULT_OK;
        }
    return FACTORY_RESULT_ENTITY_NOT_FOUND;
}

FactoryResult factory_simulation_get_fluid_port_slot(
    const FactorySimulation *simulation, FactoryEntityId id,
    FactoryFluidStorageSlot slot, FactoryFluidPortInspection *out_port
)
{
    if (simulation == NULL || id == 0U || out_port == NULL)
        return FACTORY_RESULT_INVALID_ARGUMENT;
    for (size_t i = 0U; i < simulation->fluid_networks.port_count; ++i)
        if (simulation->fluid_networks.ports[i].owner_entity_id == id
            && simulation->fluid_networks.ports[i].storage_slot == slot) {
            *out_port = simulation->fluid_networks.ports[i];
            return FACTORY_RESULT_OK;
        }
    return FACTORY_RESULT_ENTITY_NOT_FOUND;
}

size_t factory_simulation_get_fluid_network_count(
    const FactorySimulation *simulation
)
{
    return simulation == NULL ? 0U : simulation->fluid_networks.network_count;
}

FactoryResult factory_simulation_get_fluid_network(
    const FactorySimulation *simulation, size_t index,
    FactoryFluidNetworkInspection *out_network
)
{
    if (simulation == NULL || out_network == NULL)
        return FACTORY_RESULT_INVALID_ARGUMENT;
    if (index >= simulation->fluid_networks.network_count)
        return FACTORY_RESULT_FLUID_NETWORK_NOT_FOUND;
    *out_network = simulation->fluid_networks.networks[index];
    return FACTORY_RESULT_OK;
}

FactoryResult factory_simulation_submit_fluid_insert(
    FactorySimulation *simulation,
    FactoryEntityId destination_entity_id,
    FactoryFluidType fluid_type,
    FactoryFluidQuantity quantity
)
{
    FactoryCommand command = {
        FACTORY_COMMAND_FLUID_INSERT,
        {.fluid_insert = {destination_entity_id, fluid_type, quantity}}
    };
    return factory_simulation_submit_command(simulation, &command);
}

FactoryResult factory_simulation_submit_fluid_remove(
    FactorySimulation *simulation,
    FactoryEntityId source_entity_id,
    FactoryFluidQuantity quantity
)
{
    FactoryCommand command = {
        FACTORY_COMMAND_FLUID_REMOVE,
        {.fluid_remove = {source_entity_id, quantity}}
    };
    return factory_simulation_submit_command(simulation, &command);
}

FactoryResult factory_simulation_submit_fluid_transfer(
    FactorySimulation *simulation,
    FactoryEntityId source_entity_id,
    FactoryEntityId destination_entity_id,
    FactoryFluidQuantity quantity
)
{
    FactoryCommand command = {
        FACTORY_COMMAND_FLUID_TRANSFER,
        {.fluid_transfer = {
            source_entity_id, destination_entity_id, quantity
        }}
    };
    return factory_simulation_submit_command(simulation, &command);
}
