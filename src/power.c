#include "power_internal.h"

#include "simulation_internal.h"
#include "tick_preflight_internal.h"

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
    free(state->accumulators);
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

static int compare_accumulators(const void *a, const void *b)
{
    const FactoryPowerAccumulatorInspection *x = a;
    const FactoryPowerAccumulatorInspection *y = b;
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
    const FactorySteamCondenser *sc =
        factory_steam_condenser_store_find(&simulation->steam_condensers, id);
    if (e != NULL) { *out_x = e->x; *out_y = e->y; return true; }
    if (r != NULL) { *out_x = r->x; *out_y = r->y; return true; }
    if (a != NULL) { *out_x = a->x; *out_y = a->y; return true; }
    if (i != NULL) { *out_x = i->x; *out_y = i->y; return true; }
    if (sc != NULL) { *out_x = sc->x; *out_y = sc->y; return true; }
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

/*
 * The indivisible output unit a generator must produce in, rather than a
 * bounded/divisible amount up to its availability. Zero means the generator
 * is continuous (any amount up to its availability is usable). This is the
 * single reusable touchpoint atomic generators register through; adding a
 * future atomic generator type means teaching only this function (and its
 * own consume function) about it, not the allocation algorithm itself.
 */
static FactoryPowerUnits generator_quantum(
    const FactorySimulation *simulation, FactoryEntityId entity_id
)
{
    const FactorySteamTurbine *turbine = factory_steam_turbine_store_find(
        &simulation->steam_turbines, entity_id);
    const FactorySteamTurbineDefinition *definition;
    if (turbine == NULL) return 0U;
    definition = factory_steam_turbine_definition_get(turbine->definition_id);
    return definition == NULL ? 0U : definition->energy_per_cycle;
}

/*
 * See power_internal.h for the contract. This is the sole place the
 * ascending-ID-with-replacement rule is implemented; factory_power_rebuild
 * calls it once per consumer and nothing else re-derives an allocation.
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
)
{
    FactoryPowerUnits need = demand;
    size_t j;
    for (j = 0U; j < generator_count; ++j) committed_delta[j] = 0U;
    for (j = 0U; j < generator_count && need != 0U; ++j) {
        FactoryPowerGeneratorInspection *g = &generators[j];
        FactoryPowerUnits amount;
        if (g->network_id != network_id) continue;
        if (quantum[j] != 0U && !unlocked[j]) {
            if (available[j] == 0U) continue;
            if (available[j] >= demand) {
                size_t discard;
                for (discard = 0U; discard < generator_count; ++discard)
                    committed_delta[discard] = 0U;
                committed_delta[j] = demand;
                need = 0U;
                break;
            }
            committed_delta[j] = available[j] < need ? available[j] : need;
            need -= committed_delta[j];
            continue;
        }
        amount = available[j] - g->committed_output;
        if (amount == 0U) continue;
        if (amount > need) amount = need;
        committed_delta[j] = amount;
        need -= amount;
    }
    if (need != 0U) return false;
    for (j = 0U; j < generator_count; ++j) {
        if (committed_delta[j] == 0U) continue;
        generators[j].committed_output += committed_delta[j];
        if (quantum[j] != 0U) unlocked[j] = true;
    }
    return true;
}

FactoryPowerUnits factory_power_source_available_generation(
    const FactorySimulation *simulation, FactoryEntityId entity_id
)
{
    const FactoryPowerGenerator *generator;
    const FactoryBurner *burner;
    if (simulation == NULL) return 0U;
    generator = factory_power_generator_store_find(
        &simulation->power_generators, entity_id
    );
    if (generator == NULL) return 0U;
    if (factory_steam_engine_store_find(
            &simulation->steam_engines, entity_id) != NULL)
        return factory_steam_engine_available_generation(
            simulation, entity_id);
    if (factory_steam_turbine_store_find(
            &simulation->steam_turbines,entity_id)!=NULL)
        return factory_steam_turbine_available_generation(
            simulation,entity_id);
    if (factory_solar_generator_store_find(
            &simulation->solar_generators, entity_id) != NULL)
        return factory_solar_generator_available(simulation, entity_id);
    burner = factory_burner_store_find(&simulation->burners, entity_id);
    return burner == NULL
        ? 0U
        : burner->released_energy < generator->generation_capacity
            ? (FactoryPowerUnits)burner->released_energy
            : generator->generation_capacity;
}

/*
 * Execute the exact per-generator plan factory_power_rebuild committed --
 * no network aggregate, no threshold walk, no re-derivation. A continuous
 * generator with committed_output > 0 produces exactly that amount. An
 * atomic generator with committed_output > 0 fires its one complete,
 * fixed-size cycle regardless of how much of that cycle consumers and
 * accumulator charging actually claimed between them; the difference
 * between the fired quantum and committed_output is exactly what
 * factory_power_rebuild already recorded as unused_generation.
 */
void factory_power_consume_generation(FactorySimulation *simulation)
{
    size_t generator_index;
    if (simulation == NULL) return;
    for (generator_index = 0U;
        generator_index < simulation->power.generator_count;
        ++generator_index) {
        FactoryPowerGeneratorInspection *inspection =
            &simulation->power.generators[generator_index];
        FactoryPowerUnits quantum;
        if (inspection->committed_output == 0U) continue;
        quantum = generator_quantum(simulation, inspection->entity_id);
        if (quantum != 0U) {
            (void)factory_steam_turbine_consume_for_generation(
                simulation, inspection->entity_id, quantum);
            continue;
        }
        if (factory_steam_engine_store_find(
                &simulation->steam_engines,
                inspection->entity_id) != NULL) {
            (void)factory_steam_engine_consume_for_generation(
                simulation, inspection->entity_id,
                inspection->committed_output);
        } else if (factory_solar_generator_store_find(
                &simulation->solar_generators,
                inspection->entity_id) != NULL) {
            (void)factory_solar_generator_record_generation(
                simulation, inspection->entity_id,
                inspection->committed_output);
        } else {
            FactoryBurner *burner = factory_burner_store_find_mutable(
                &simulation->burners, inspection->entity_id);
            (void)factory_burner_consume_energy(
                burner, inspection->committed_output);
        }
    }
}

FactoryResult factory_power_rebuild(
    FactorySimulation *simulation, bool emit_transitions
)
{
    FactoryPowerState next = {0};
    size_t i;
    size_t j;
    size_t consumers = simulation->extractors.count
        + simulation->refineries.count + simulation->assemblers.count
        + simulation->inserters.count + simulation->steam_condensers.count;
    /*
     * Per-generator plan, alive for the whole rebuild: consumer allocation
     * and accumulator-charge attribution both write into
     * next.generators[*].committed_output, so factory_power_consume_
     * generation later has one authoritative number per generator instead
     * of re-deriving anything from a network aggregate.
     */
    FactoryPowerUnits *available = NULL;
    FactoryPowerUnits *quantum = NULL;
    bool *unlocked = NULL;
    FactoryPowerUnits *committed_delta = NULL;

    next.pole_count = simulation->power_poles.count;
    next.generator_count = simulation->power_generators.count;
    next.accumulator_count = simulation->accumulators.count;
    next.consumer_count = consumers;
    if ((next.pole_count != 0U
            && (next.poles = factory_topology_calloc(simulation,
                FACTORY_TOPOLOGY_POWER,next.pole_count,sizeof(*next.poles)))
                == NULL)
        || (next.generator_count != 0U
            && (next.generators = factory_topology_calloc(simulation,
                FACTORY_TOPOLOGY_POWER,next.generator_count,
                sizeof(*next.generators))) == NULL)
        || (next.accumulator_count != 0U
            && (next.accumulators = factory_topology_calloc(simulation,
                FACTORY_TOPOLOGY_POWER,next.accumulator_count,
                sizeof(*next.accumulators))) == NULL)
        || (consumers != 0U
            && (next.consumers = factory_topology_calloc(simulation,
                FACTORY_TOPOLOGY_POWER,consumers,
                sizeof(*next.consumers))) == NULL)) {
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
        next.connections = factory_topology_calloc(simulation,
            FACTORY_TOPOLOGY_POWER,maximum,sizeof(*next.connections));
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
        next.networks = factory_topology_calloc(simulation,
            FACTORY_TOPOLOGY_POWER,next.pole_count,sizeof(*next.networks));
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
            0U, FACTORY_POWER_NETWORK_NONE, false, 0U
        };
    }
    for (i = 0U; i < next.accumulator_count; ++i) {
        const FactoryAccumulator *a = &simulation->accumulators.items[i];
        next.accumulators[i] = (FactoryPowerAccumulatorInspection){
            a->entity_id, 0U, FACTORY_POWER_NETWORK_NONE, false};
    }
    if (next.accumulator_count > 1U)
        qsort(next.accumulators, next.accumulator_count,
              sizeof(*next.accumulators), compare_accumulators);
    for (i = 0U; i < next.accumulator_count; ++i) {
        FactoryPowerAccumulatorInspection *a = &next.accumulators[i];
        const FactoryAccumulator *component =
            factory_accumulator_store_find(
                &simulation->accumulators, a->entity_id);
        FactoryPowerNetworkInspection *network;
        a->attached_pole_id =
            attachment(&next, component->x, component->y);
        a->network_id = pole_network(&next, a->attached_pole_id);
        a->connected = a->attached_pole_id != 0U;
        network = network_for(&next, a->network_id);
        if (network != NULL) ++network->accumulator_count;
    }
    if (next.generator_count > 1U) {
        qsort(
            next.generators, next.generator_count,
            sizeof(*next.generators), compare_generators
        );
    }
    if (next.generator_count != 0U
        && ((available = factory_topology_calloc(simulation,
                FACTORY_TOPOLOGY_POWER,next.generator_count,
                sizeof(*available)))
                == NULL
            || (quantum = factory_topology_calloc(simulation,
                FACTORY_TOPOLOGY_POWER,next.generator_count,sizeof(*quantum)))
                == NULL
            || (unlocked = factory_topology_calloc(simulation,
                FACTORY_TOPOLOGY_POWER,next.generator_count,sizeof(*unlocked)))
                == NULL
            || (committed_delta = factory_topology_calloc(simulation,
                    FACTORY_TOPOLOGY_POWER,next.generator_count,
                    sizeof(*committed_delta)))
                == NULL)) {
        free(available); free(quantum); free(unlocked);
        free(committed_delta);
        factory_power_state_destroy(&next);
        return FACTORY_RESULT_OUT_OF_MEMORY;
    }
    for (i = 0U; i < next.generator_count; ++i) {
        FactoryPowerGeneratorInspection *g = &next.generators[i];
        FactoryPowerNetworkInspection *network;
        g->attached_pole_id = attachment(&next, g->x, g->y);
        g->network_id = pole_network(&next, g->attached_pole_id);
        g->connected = g->attached_pole_id != 0U;
        available[i] = factory_power_source_available_generation(
            simulation, g->entity_id);
        quantum[i] = generator_quantum(simulation, g->entity_id);
        network = network_for(&next, g->network_id);
        if (network == NULL) continue;
        ++network->generator_count;
        /*
         * Continuous capacity counts toward total_generation up front: it
         * is available regardless of whether anything ends up using it. An
         * atomic generator's capacity is conditional on actually firing, so
         * it is added back in only for the generators the allocation below
         * commits to.
         */
        if (quantum[i] != 0U) continue;
        if (network->total_generation > UINT64_MAX - available[i]) {
            free(available); free(quantum); free(unlocked);
            free(committed_delta);
            factory_power_state_destroy(&next);
            return FACTORY_RESULT_POWER_OVERFLOW;
        }
        network->total_generation += available[i];
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
    for (j = 0U; j < simulation->steam_condensers.count; ++j)
        add_consumer(&next.consumers[i++],
            simulation->steam_condensers.items[j].entity_id,
            FACTORY_POWER_DEMAND_STEAM_CONDENSER);
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
        if (network == NULL) continue;
        if (network->total_demand > UINT64_MAX - c->demand) {
            free(available); free(quantum); free(unlocked);
            free(committed_delta);
            factory_power_state_destroy(&next);
            return FACTORY_RESULT_POWER_OVERFLOW;
        }
        network->total_demand += c->demand;
        ++network->consumer_count;
    }
    /*
     * Ascending entity ID remains the default priority across every
     * generator, continuous or atomic alike -- there is no global
     * atomic-before-continuous rule; see factory_power_allocate_consumer
     * for the per-consumer construction/replacement rule. Consumers
     * themselves are processed in that same ascending order, and each
     * one's plan commits only once it reaches its complete demand.
     */
    for (i = 0U; i < consumers; ++i) {
        FactoryPowerConsumerInspection *c = &next.consumers[i];
        FactoryPowerNetworkInspection *network =
            network_for(&next, c->network_id);
        if (network == NULL) continue;
        if (!factory_power_allocate_consumer(
                next.generators, available, quantum, unlocked,
                next.generator_count, c->network_id, c->demand,
                committed_delta))
            continue;
        c->powered = true;
        network->allocated_power += c->demand;
        ++network->powered_consumer_count;
    }
    for (i = 0U; i < next.generator_count; ++i) {
        FactoryPowerNetworkInspection *network;
        if (quantum[i] == 0U || !unlocked[i]) continue;
        network = network_for(&next, next.generators[i].network_id);
        if (network == NULL) continue;
        if (network->total_generation > UINT64_MAX - available[i]) {
            free(available); free(quantum); free(unlocked);
            free(committed_delta);
            factory_power_state_destroy(&next);
            return FACTORY_RESULT_POWER_OVERFLOW;
        }
        network->total_generation += available[i];
    }
    if (emit_transitions) for (i = 0U; i < consumers; ++i) {
        FactoryPowerConsumerInspection *c = &next.consumers[i];
        FactoryPowerNetworkInspection *network =
            network_for(&next, c->network_id);
        FactoryPowerTotal usable = 0U;
        FactoryPowerUnits needed;
        if (network == NULL || c->powered) continue;
        needed = c->demand;
        for (j = 0U; j < next.accumulator_count && usable < needed; ++j) {
            FactoryPowerAccumulatorInspection *inspection =
                &next.accumulators[j];
            FactoryAccumulator *a;
            FactoryPowerUnits available;
            if (inspection->network_id != c->network_id) continue;
            a = factory_accumulator_store_find_mutable(
                &simulation->accumulators, inspection->entity_id);
            if (a == NULL || a->charged_last_tick != 0U) continue;
            available = FACTORY_ACCUMULATOR_MAX_DISCHARGE_RATE
                - a->discharged_last_tick;
            if (a->stored_energy < available)
                available = (FactoryPowerUnits)a->stored_energy;
            usable += available;
        }
        if (usable < needed) continue;
        for (j = 0U; j < next.accumulator_count && needed != 0U; ++j) {
            FactoryPowerAccumulatorInspection *inspection =
                &next.accumulators[j];
            FactoryAccumulator *a;
            FactoryPowerUnits amount;
            if (inspection->network_id != c->network_id) continue;
            a = factory_accumulator_store_find_mutable(
                &simulation->accumulators, inspection->entity_id);
            if (a == NULL || a->charged_last_tick != 0U) continue;
            amount = FACTORY_ACCUMULATOR_MAX_DISCHARGE_RATE
                - a->discharged_last_tick;
            if (a->stored_energy < amount)
                amount = (FactoryPowerUnits)a->stored_energy;
            if (amount > needed) amount = needed;
            a->stored_energy -= amount;
            a->discharged_last_tick += amount;
            needed -= amount;
            network->accumulator_discharge += amount;
        }
        c->powered = true;
        network->allocated_power += c->demand;
        ++network->powered_consumer_count;
    }
    for (i = 0U; i < next.consumer_count; ++i) {
        FactoryPowerConsumerInspection *c = &next.consumers[i];
        FactoryPowerNetworkInspection *network =
            network_for(&next, c->network_id);
        if (network != NULL && !c->powered)
            ++network->unpowered_consumer_count;
    }
    /*
     * Accumulator charging is attributed to specific generators, not drawn
     * from a network aggregate: for each accumulator (ascending ID), walk
     * generators in the same ascending order used everywhere else and add
     * the charge directly to their committed_output, exactly like a
     * consumer's delivery. This never fires an atomic generator solely to
     * charge a battery -- only a generator already producing (continuous,
     * or an atomic generator a consumer already triggered this tick) is a
     * source -- so charging never changes the fire/no-fire decision, only
     * how much of what is already being produced gets claimed.
     */
    if (emit_transitions) for (i = 0U; i < next.network_count; ++i) {
        FactoryPowerNetworkInspection *network = &next.networks[i];
        for (j = 0U; j < next.accumulator_count; ++j) {
            FactoryPowerAccumulatorInspection *inspection =
                &next.accumulators[j];
            FactoryAccumulator *a;
            FactoryElectricalEnergy capacity;
            FactoryPowerUnits target;
            FactoryPowerUnits need;
            size_t k;
            if (inspection->network_id != network->network_id) continue;
            a = factory_accumulator_store_find_mutable(
                &simulation->accumulators, inspection->entity_id);
            if (a == NULL || a->discharged_last_tick != 0U) continue;
            capacity = FACTORY_ACCUMULATOR_CAPACITY - a->stored_energy;
            target = FACTORY_ACCUMULATOR_MAX_CHARGE_RATE;
            if (capacity < target) target = (FactoryPowerUnits)capacity;
            need = target;
            for (k = 0U; k < next.generator_count && need != 0U; ++k) {
                FactoryPowerGeneratorInspection *g = &next.generators[k];
                FactoryPowerUnits leftover;
                FactoryPowerUnits amount;
                if (g->network_id != network->network_id) continue;
                if (quantum[k] != 0U && !unlocked[k]) continue;
                leftover = available[k] - g->committed_output;
                if (leftover == 0U) continue;
                amount = leftover < need ? leftover : need;
                g->committed_output += amount;
                need -= amount;
            }
            if (need == target) continue;
            {
                FactoryPowerUnits charged = target - need;
                a->stored_energy += charged;
                a->charged_last_tick += charged;
                network->accumulator_charge += charged;
            }
        }
    }
    for (i = 0U; i < next.network_count; ++i) {
        FactoryPowerNetworkInspection *network = &next.networks[i];
        FactoryPowerTotal committed_total = 0U;
        for (j = 0U; j < next.generator_count; ++j) {
            if (next.generators[j].network_id != network->network_id)
                continue;
            committed_total += next.generators[j].committed_output;
        }
        network->unused_generation =
            network->total_generation - committed_total;
    }
    free(available);
    free(quantum);
    free(unlocked);
    free(committed_delta);
    if (emit_transitions) {
        for (i = 0U; i < next.accumulator_count; ++i) {
            FactoryAccumulator *a = factory_accumulator_store_find_mutable(
                &simulation->accumulators, next.accumulators[i].entity_id);
            if (a->charged_last_tick != 0U)
                factory_simulation_emit_event(simulation, (FactoryEvent){
                    .type = FACTORY_EVENT_ACCUMULATOR_CHARGED,
                    .entity_id = a->entity_id,
                    .quantity = a->charged_last_tick,
                    .related_quantity = (uint32_t)a->stored_energy});
            else if (a->discharged_last_tick != 0U)
                factory_simulation_emit_event(simulation, (FactoryEvent){
                    .type = FACTORY_EVENT_ACCUMULATOR_DISCHARGED,
                    .entity_id = a->entity_id,
                    .quantity = a->discharged_last_tick,
                    .related_quantity = (uint32_t)a->stored_energy});
        }
    }
    if (emit_transitions) {
        for (i = 0U; i < next.consumer_count; ++i) {
            const FactoryPowerConsumerInspection *current =
                &next.consumers[i];
            size_t previous_index;
            for (previous_index = 0U;
                previous_index < simulation->power.consumer_count;
                ++previous_index) {
                const FactoryPowerConsumerInspection *previous =
                    &simulation->power.consumers[previous_index];
                if (previous->entity_id != current->entity_id) continue;
                if (previous->powered != current->powered) {
                    factory_simulation_emit_event(
                        simulation, (FactoryEvent){
                            .type = current->powered
                                ? FACTORY_EVENT_POWER_GAINED
                                : FACTORY_EVENT_POWER_LOST,
                            .entity_id = current->entity_id
                        }
                    );
                }
                break;
            }
        }
    }
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
