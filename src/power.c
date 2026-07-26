#include "power_internal.h"

#include "simulation_internal.h"

#include <stdlib.h>

static bool reserve(void **items, size_t *capacity, size_t count, size_t width)
{
    void *resized;
    size_t next;
    if (count < *capacity) return true;
    next = *capacity == 0U ? 4U : *capacity * 2U;
    if (next < *capacity || next > SIZE_MAX / width) return false;
    resized = realloc(*items, next * width);
    if (resized == NULL) return false;
    *items = resized;
    *capacity = next;
    return true;
}

void factory_power_pole_store_destroy(FactoryPowerPoleStore *store)
{
    if (store == NULL) return;
    free(store->items);
    *store = (FactoryPowerPoleStore){0};
}

void factory_power_generator_store_destroy(FactoryPowerGeneratorStore *store)
{
    if (store == NULL) return;
    free(store->items);
    *store = (FactoryPowerGeneratorStore){0};
}

bool factory_power_pole_store_reserve_one(FactoryPowerPoleStore *store)
{
    return store != NULL && reserve(
        (void **)&store->items, &store->capacity,
        store->count, sizeof(*store->items)
    );
}

bool factory_power_generator_store_reserve_one(FactoryPowerGeneratorStore *store)
{
    return store != NULL && reserve(
        (void **)&store->items, &store->capacity,
        store->count, sizeof(*store->items)
    );
}

void factory_power_pole_store_add(
    FactoryPowerPoleStore *store, FactoryEntityId id, int32_t x, int32_t y
)
{
    store->items[store->count++] = (FactoryPowerPole){id, x, y};
}

void factory_power_generator_store_add(
    FactoryPowerGeneratorStore *store,
    FactoryEntityId id,
    int32_t x,
    int32_t y
)
{
    store->items[store->count++] = (FactoryPowerGenerator){
        id, x, y, FACTORY_BASIC_GENERATOR_CAPACITY
    };
}

#define DEFINE_FIND(NAME, TYPE, STORE)                                      \
const TYPE *NAME(const STORE *store, FactoryEntityId id)                    \
{                                                                            \
    size_t i;                                                                \
    if (store == NULL || id == 0U) return NULL;                              \
    for (i = 0U; i < store->count; ++i)                                     \
        if (store->items[i].entity_id == id) return &store->items[i];        \
    return NULL;                                                             \
}

DEFINE_FIND(factory_power_pole_store_find, FactoryPowerPole, FactoryPowerPoleStore)
DEFINE_FIND(factory_power_generator_store_find, FactoryPowerGenerator, FactoryPowerGeneratorStore)

#define DEFINE_REMOVE(NAME, STORE)                                           \
bool NAME(STORE *store, FactoryEntityId id)                                  \
{                                                                            \
    size_t i;                                                                \
    if (store == NULL) return false;                                         \
    for (i = 0U; i < store->count; ++i) {                                   \
        if (store->items[i].entity_id == id) {                               \
            --store->count;                                                  \
            store->items[i] = store->items[store->count];                    \
            return true;                                                     \
        }                                                                    \
    }                                                                        \
    return false;                                                            \
}

DEFINE_REMOVE(factory_power_pole_store_remove, FactoryPowerPoleStore)
DEFINE_REMOVE(factory_power_generator_store_remove, FactoryPowerGeneratorStore)

void factory_power_state_destroy(FactoryPowerState *state)
{
    if (state == NULL) return;
    free(state->poles);
    free(state->generators);
    free(state->consumers);
    free(state->connections);
    free(state->networks);
    *state = (FactoryPowerState){0};
}

static uint32_t grid_distance(
    int32_t ax, int32_t ay, int32_t bx, int32_t by
)
{
    int64_t dx = (int64_t)ax - bx;
    int64_t dy = (int64_t)ay - by;
    uint64_t ux = (uint64_t)(dx < 0 ? -dx : dx);
    uint64_t uy = (uint64_t)(dy < 0 ? -dy : dy);
    uint64_t distance = ux > uy ? ux : uy;
    return distance > UINT32_MAX ? UINT32_MAX : (uint32_t)distance;
}

static int compare_poles(const void *a, const void *b)
{
    const FactoryPowerPoleInspection *x = a;
    const FactoryPowerPoleInspection *y = b;
    return x->entity_id < y->entity_id ? -1 : x->entity_id > y->entity_id;
}

static int compare_generators(const void *a, const void *b)
{
    const FactoryPowerGeneratorInspection *x = a;
    const FactoryPowerGeneratorInspection *y = b;
    return x->entity_id < y->entity_id ? -1 : x->entity_id > y->entity_id;
}

static int compare_consumers(const void *a, const void *b)
{
    const FactoryPowerConsumerInspection *x = a;
    const FactoryPowerConsumerInspection *y = b;
    return x->entity_id < y->entity_id ? -1 : x->entity_id > y->entity_id;
}

static FactoryEntityId attachment(
    const FactoryPowerState *state, int32_t x, int32_t y
)
{
    size_t i;
    for (i = 0U; i < state->pole_count; ++i) {
        if (grid_distance(
                x, y, state->poles[i].x, state->poles[i].y)
            <= FACTORY_POWER_POLE_MACHINE_RADIUS) {
            return state->poles[i].entity_id;
        }
    }
    return 0U;
}

static FactoryPowerNetworkId pole_network(
    const FactoryPowerState *state, FactoryEntityId pole
)
{
    size_t i;
    for (i = 0U; i < state->pole_count; ++i)
        if (state->poles[i].entity_id == pole) return state->poles[i].network_id;
    return FACTORY_POWER_NETWORK_NONE;
}

static void add_consumer(
    FactoryPowerConsumerInspection *out,
    FactoryEntityId id,
    FactoryPowerUnits demand
)
{
    *out = (FactoryPowerConsumerInspection){
        id, demand, 0U, FACTORY_POWER_NETWORK_NONE, false, false
    };
}

static bool consumer_position(
    const FactorySimulation *simulation,
    FactoryEntityId id,
    int32_t *out_x,
    int32_t *out_y
)
{
    const FactoryExtractor *e =
        factory_extractor_store_find(&simulation->extractors, id);
    const FactoryRefinery *r =
        factory_refinery_store_find(&simulation->refineries, id);
    const FactoryAssembler *a =
        factory_assembler_store_find(&simulation->assemblers, id);
    const FactoryInserter *i =
        factory_inserter_store_find(&simulation->inserters, id);
    if (e != NULL) { *out_x = e->x; *out_y = e->y; return true; }
    if (r != NULL) { *out_x = r->x; *out_y = r->y; return true; }
    if (a != NULL) { *out_x = a->x; *out_y = a->y; return true; }
    if (i != NULL) { *out_x = i->x; *out_y = i->y; return true; }
    return false;
}

static FactoryPowerNetworkInspection *network_for(
    FactoryPowerState *state, FactoryPowerNetworkId id
)
{
    size_t i;
    for (i = 0U; i < state->network_count; ++i)
        if (state->networks[i].network_id == id) return &state->networks[i];
    return NULL;
}

FactoryPowerUnits factory_power_source_available_generation(
    const FactorySimulation *simulation, FactoryEntityId entity_id
)
{
    const FactoryPowerGenerator *generator;
    if (simulation == NULL) return 0U;
    generator = factory_power_generator_store_find(
        &simulation->power_generators, entity_id
    );
    return generator == NULL ? 0U : generator->generation_capacity;
}

FactoryResult factory_power_rebuild(FactorySimulation *simulation)
{
    FactoryPowerState next = {0};
    size_t i;
    size_t j;
    size_t consumers = simulation->extractors.count
        + simulation->refineries.count + simulation->assemblers.count
        + simulation->inserters.count;

    next.pole_count = simulation->power_poles.count;
    next.generator_count = simulation->power_generators.count;
    next.consumer_count = consumers;
    if ((next.pole_count != 0U
            && (next.poles = calloc(next.pole_count, sizeof(*next.poles)))
                == NULL)
        || (next.generator_count != 0U
            && (next.generators = calloc(
                next.generator_count, sizeof(*next.generators))) == NULL)
        || (consumers != 0U
            && (next.consumers = calloc(
                consumers, sizeof(*next.consumers))) == NULL)) {
        factory_power_state_destroy(&next);
        return FACTORY_RESULT_OUT_OF_MEMORY;
    }
    for (i = 0U; i < next.pole_count; ++i) {
        const FactoryPowerPole *p = &simulation->power_poles.items[i];
        next.poles[i] = (FactoryPowerPoleInspection){
            p->entity_id, p->x, p->y,
            FACTORY_POWER_POLE_MACHINE_RADIUS,
            FACTORY_POWER_POLE_WIRE_RADIUS,
            p->entity_id, 0U
        };
    }
    if (next.pole_count > 1U) {
        qsort(next.poles, next.pole_count, sizeof(*next.poles), compare_poles);
    }
    if (next.pole_count > 1U) {
        size_t maximum;
        if (next.pole_count > SIZE_MAX / (next.pole_count - 1U)) {
            factory_power_state_destroy(&next);
            return FACTORY_RESULT_POWER_OVERFLOW;
        }
        maximum = next.pole_count * (next.pole_count - 1U) / 2U;
        next.connections = calloc(maximum, sizeof(*next.connections));
        if (next.connections == NULL) {
            factory_power_state_destroy(&next);
            return FACTORY_RESULT_OUT_OF_MEMORY;
        }
        for (i = 0U; i < next.pole_count; ++i) {
            for (j = i + 1U; j < next.pole_count; ++j) {
                if (grid_distance(
                        next.poles[i].x, next.poles[i].y,
                        next.poles[j].x, next.poles[j].y)
                    <= FACTORY_POWER_POLE_WIRE_RADIUS) {
                    next.connections[next.connection_count++] =
                        (FactoryPowerConnectionInspection){
                            next.poles[i].entity_id,
                            next.poles[j].entity_id
                        };
                    ++next.poles[i].connected_pole_count;
                    ++next.poles[j].connected_pole_count;
                }
            }
        }
        for (i = 0U; i < next.pole_count; ++i) {
            bool changed;
            do {
                changed = false;
                for (j = 0U; j < next.connection_count; ++j) {
                    FactoryPowerConnectionInspection edge =
                        next.connections[j];
                    size_t a;
                    size_t b;
                    for (a = 0U; next.poles[a].entity_id != edge.pole_a; ++a) {}
                    for (b = 0U; next.poles[b].entity_id != edge.pole_b; ++b) {}
                    {
                        FactoryPowerNetworkId minimum =
                            next.poles[a].network_id
                                < next.poles[b].network_id
                            ? next.poles[a].network_id
                            : next.poles[b].network_id;
                        if (next.poles[a].network_id != minimum
                            || next.poles[b].network_id != minimum) {
                            next.poles[a].network_id = minimum;
                            next.poles[b].network_id = minimum;
                            changed = true;
                        }
                    }
                }
            } while (changed);
            break;
        }
    }
    if (next.pole_count != 0U) {
        next.networks = calloc(next.pole_count, sizeof(*next.networks));
        if (next.networks == NULL) {
            factory_power_state_destroy(&next);
            return FACTORY_RESULT_OUT_OF_MEMORY;
        }
        for (i = 0U; i < next.pole_count; ++i) {
            FactoryPowerNetworkInspection *network =
                network_for(&next, next.poles[i].network_id);
            if (network == NULL) {
                network = &next.networks[next.network_count++];
                network->network_id = next.poles[i].network_id;
            }
            ++network->pole_count;
        }
    }
    for (i = 0U; i < next.generator_count; ++i) {
        const FactoryPowerGenerator *g = &simulation->power_generators.items[i];
        next.generators[i] = (FactoryPowerGeneratorInspection){
            g->entity_id, g->x, g->y, g->generation_capacity,
            0U, FACTORY_POWER_NETWORK_NONE, false
        };
    }
    if (next.generator_count > 1U) {
        qsort(
            next.generators, next.generator_count,
            sizeof(*next.generators), compare_generators
        );
    }
    for (i = 0U; i < next.generator_count; ++i) {
        FactoryPowerGeneratorInspection *g = &next.generators[i];
        FactoryPowerNetworkInspection *network;
        FactoryPowerUnits available;
        g->attached_pole_id = attachment(&next, g->x, g->y);
        g->network_id = pole_network(&next, g->attached_pole_id);
        g->connected = g->attached_pole_id != 0U;
        network = network_for(&next, g->network_id);
        if (network != NULL) {
            available = factory_power_source_available_generation(
                simulation, g->entity_id
            );
            if (network->total_generation
                > UINT64_MAX - available) {
                factory_power_state_destroy(&next);
                return FACTORY_RESULT_POWER_OVERFLOW;
            }
            network->total_generation += available;
            ++network->generator_count;
        }
    }
    i = 0U;
    for (j = 0U; j < simulation->extractors.count; ++j)
        add_consumer(&next.consumers[i++],
            simulation->extractors.items[j].entity_id,
            FACTORY_POWER_DEMAND_EXTRACTOR);
    for (j = 0U; j < simulation->refineries.count; ++j)
        add_consumer(&next.consumers[i++],
            simulation->refineries.items[j].entity_id,
            FACTORY_POWER_DEMAND_REFINERY);
    for (j = 0U; j < simulation->assemblers.count; ++j)
        add_consumer(&next.consumers[i++],
            simulation->assemblers.items[j].entity_id,
            FACTORY_POWER_DEMAND_ASSEMBLER);
    for (j = 0U; j < simulation->inserters.count; ++j)
        add_consumer(&next.consumers[i++],
            simulation->inserters.items[j].entity_id,
            FACTORY_POWER_DEMAND_INSERTER);
    if (consumers > 1U) {
        qsort(
            next.consumers, consumers,
            sizeof(*next.consumers), compare_consumers
        );
    }
    for (i = 0U; i < consumers; ++i) {
        FactoryPowerConsumerInspection *c = &next.consumers[i];
        FactoryPowerNetworkInspection *network;
        int32_t x = 0;
        int32_t y = 0;
        (void)consumer_position(simulation, c->entity_id, &x, &y);
        c->attached_pole_id = attachment(&next, x, y);
        c->network_id = pole_network(&next, c->attached_pole_id);
        c->connected = c->attached_pole_id != 0U;
        network = network_for(&next, c->network_id);
        if (network != NULL) {
            if (network->total_demand > UINT64_MAX - c->demand) {
                factory_power_state_destroy(&next);
                return FACTORY_RESULT_POWER_OVERFLOW;
            }
            network->total_demand += c->demand;
            ++network->consumer_count;
        }
    }
    for (i = 0U; i < consumers; ++i) {
        FactoryPowerConsumerInspection *c = &next.consumers[i];
        FactoryPowerNetworkInspection *network =
            network_for(&next, c->network_id);
        if (network != NULL
            && c->demand
                <= network->total_generation - network->allocated_power) {
            c->powered = true;
            network->allocated_power += c->demand;
            ++network->powered_consumer_count;
        } else if (network != NULL) {
            ++network->unpowered_consumer_count;
        }
    }
    for (i = 0U; i < next.network_count; ++i)
        next.networks[i].unused_generation =
            next.networks[i].total_generation
            - next.networks[i].allocated_power;
    factory_power_state_destroy(&simulation->power);
    simulation->power = next;
    return FACTORY_RESULT_OK;
}

bool factory_power_is_entity_powered(
    const FactorySimulation *simulation, FactoryEntityId entity_id
)
{
    size_t i;
    if (simulation == NULL) return false;
    for (i = 0U; i < simulation->power.consumer_count; ++i)
        if (simulation->power.consumers[i].entity_id == entity_id)
            return simulation->power.consumers[i].powered;
    return false;
}

FactoryResult factory_simulation_get_power_pole(
    const FactorySimulation *simulation, FactoryEntityId id,
    FactoryPowerPoleInspection *out
)
{
    size_t i;
    if (simulation == NULL || out == NULL) return FACTORY_RESULT_INVALID_ARGUMENT;
    for (i = 0U; i < simulation->power.pole_count; ++i)
        if (simulation->power.poles[i].entity_id == id) {
            *out = simulation->power.poles[i]; return FACTORY_RESULT_OK;
        }
    return FACTORY_RESULT_ENTITY_NOT_FOUND;
}

FactoryResult factory_simulation_get_power_generator(
    const FactorySimulation *simulation, FactoryEntityId id,
    FactoryPowerGeneratorInspection *out
)
{
    size_t i;
    if (simulation == NULL || out == NULL) return FACTORY_RESULT_INVALID_ARGUMENT;
    for (i = 0U; i < simulation->power.generator_count; ++i)
        if (simulation->power.generators[i].entity_id == id) {
            *out = simulation->power.generators[i]; return FACTORY_RESULT_OK;
        }
    return FACTORY_RESULT_ENTITY_NOT_FOUND;
}

FactoryResult factory_simulation_get_power_consumer(
    const FactorySimulation *simulation, FactoryEntityId id,
    FactoryPowerConsumerInspection *out
)
{
    size_t i;
    if (simulation == NULL || out == NULL) return FACTORY_RESULT_INVALID_ARGUMENT;
    for (i = 0U; i < simulation->power.consumer_count; ++i)
        if (simulation->power.consumers[i].entity_id == id) {
            *out = simulation->power.consumers[i]; return FACTORY_RESULT_OK;
        }
    return FACTORY_RESULT_POWER_NOT_APPLICABLE;
}

size_t factory_simulation_get_power_network_count(const FactorySimulation *s)
{ return s == NULL ? 0U : s->power.network_count; }

FactoryResult factory_simulation_get_power_network(
    const FactorySimulation *s, size_t i, FactoryPowerNetworkInspection *out)
{
    if (s == NULL || out == NULL) return FACTORY_RESULT_INVALID_ARGUMENT;
    if (i >= s->power.network_count) return FACTORY_RESULT_POWER_NETWORK_NOT_FOUND;
    *out = s->power.networks[i]; return FACTORY_RESULT_OK;
}

size_t factory_simulation_get_power_connection_count(const FactorySimulation *s)
{ return s == NULL ? 0U : s->power.connection_count; }

FactoryResult factory_simulation_get_power_connection(
    const FactorySimulation *s, size_t i,
    FactoryPowerConnectionInspection *out)
{
    if (s == NULL || out == NULL) return FACTORY_RESULT_INVALID_ARGUMENT;
    if (i >= s->power.connection_count) return FACTORY_RESULT_OUT_OF_BOUNDS;
    *out = s->power.connections[i]; return FACTORY_RESULT_OK;
}
