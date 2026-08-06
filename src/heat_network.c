#include "heat_network_internal.h"
#include "foundation/content.h"

#include "event_internal.h"
#include "fluid_internal.h"
#include "reactor_internal.h"
#include "simulation_internal.h"
#include "tick_preflight_internal.h"

#include <stdlib.h>

const FactoryHeatExchangeRecipe *factory_heat_exchange_recipe_get(
    uint32_t recipe_id)
{
    return factory_content_heat_exchange_recipe_get(recipe_id);
}

static bool adjacent(int32_t ax, int32_t ay, int32_t bx, int32_t by)
{
    int64_t dx = (int64_t)ax - bx;
    int64_t dy = (int64_t)ay - by;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    return dx + dy == 1;
}

static int conductor_compare(const void *a, const void *b)
{
    FactoryEntityId x =
        ((const FactoryHeatConductorInspection *)a)->entity_id;
    FactoryEntityId y =
        ((const FactoryHeatConductorInspection *)b)->entity_id;
    return x < y ? -1 : x > y;
}

static int port_compare(const void *a, const void *b)
{
    const FactoryHeatPortInspection *x = a;
    const FactoryHeatPortInspection *y = b;
    if (x->owner_entity_id != y->owner_entity_id)
        return x->owner_entity_id < y->owner_entity_id ? -1 : 1;
    return x->slot < y->slot ? -1 : x->slot > y->slot;
}

static FactoryHeatNetworkInspection *network_for(
    FactoryHeatNetworkState *state, FactoryHeatNetworkId id)
{
    for (size_t i = 0U; i < state->network_count; ++i)
        if (state->networks[i].network_id == id) return &state->networks[i];
    return NULL;
}

#define DEFINE_STORE(prefix, Store, Item, idfield)                          \
void prefix##_destroy(Store *s) { if (s != NULL) { free(s->items); *s=(Store){0}; } } \
bool prefix##_reserve_one(Store *s) {                                       \
    Item *p; size_t c; if (s == NULL) return false;                          \
    if (s->count < s->capacity) return true;                                \
    c = s->capacity == 0U ? 4U : s->capacity * 2U;                          \
    if (c < s->capacity || c > SIZE_MAX / sizeof(*p)) return false;          \
    p = realloc(s->items, c * sizeof(*p)); if (p == NULL) return false;      \
    s->items = p; s->capacity = c; return true;                              \
}                                                                           \
bool prefix##_remove(Store *s, FactoryEntityId id) {                        \
    if (s == NULL) return false;                                            \
    for (size_t i=0U;i<s->count;++i) if (s->items[i].idfield == id) {       \
        --s->count; s->items[i]=s->items[s->count]; return true; }           \
    return false;                                                           \
}

DEFINE_STORE(factory_heat_conductor_store, FactoryHeatConductorStore,
    FactoryHeatConductor, entity_id)
DEFINE_STORE(factory_heat_port_store, FactoryHeatPortStore,
    FactoryHeatPort, owner_entity_id)
DEFINE_STORE(factory_heat_exchanger_store, FactoryHeatExchangerStore,
    FactoryHeatExchanger, entity_id)

void factory_heat_conductor_store_add(
    FactoryHeatConductorStore *s, FactoryEntityId id, int32_t x, int32_t y)
{
    s->items[s->count++] = (FactoryHeatConductor){id, x, y};
}

const FactoryHeatConductor *factory_heat_conductor_store_find(
    const FactoryHeatConductorStore *s, FactoryEntityId id)
{
    if (s == NULL) return NULL;
    for (size_t i=0U;i<s->count;++i)
        if (s->items[i].entity_id == id) return &s->items[i];
    return NULL;
}

void factory_heat_port_store_add(
    FactoryHeatPortStore *s, FactoryEntityId owner,
    FactoryHeatPortSlot slot, int32_t x, int32_t y)
{
    s->items[s->count++] = (FactoryHeatPort){owner, slot, x, y};
}

void factory_heat_exchanger_store_add(
    FactoryHeatExchangerStore *s, FactoryEntityId id, int32_t x, int32_t y)
{
    s->items[s->count++] = (FactoryHeatExchanger){
        .entity_id=id, .x=x, .y=y};
}

const FactoryHeatExchanger *factory_heat_exchanger_store_find(
    const FactoryHeatExchangerStore *s, FactoryEntityId id)
{
    if (s == NULL) return NULL;
    for (size_t i=0U;i<s->count;++i)
        if (s->items[i].entity_id == id) return &s->items[i];
    return NULL;
}

FactoryHeatExchanger *factory_heat_exchanger_store_find_mutable(
    FactoryHeatExchangerStore *s, FactoryEntityId id)
{
    return (FactoryHeatExchanger *)factory_heat_exchanger_store_find(s, id);
}

void factory_heat_network_state_destroy(FactoryHeatNetworkState *s)
{
    if (s == NULL) return;
    free(s->conductors); free(s->ports); free(s->networks);
    *s = (FactoryHeatNetworkState){0};
}

static uint32_t connection_bit(
    int32_t ax, int32_t ay, int32_t bx, int32_t by)
{
    if (bx == ax && by == ay - 1) return FACTORY_HEAT_CONNECTION_NORTH;
    if (bx == ax + 1 && by == ay) return FACTORY_HEAT_CONNECTION_EAST;
    if (bx == ax && by == ay + 1) return FACTORY_HEAT_CONNECTION_SOUTH;
    if (bx == ax - 1 && by == ay) return FACTORY_HEAT_CONNECTION_WEST;
    return 0U;
}

FactoryResult factory_heat_network_rebuild(
    FactorySimulation *simulation, bool emit_events)
{
    FactoryHeatNetworkState next = {0};
    FactoryHeatNetworkState *old = &simulation->heat_networks;
    size_t i;
    next.conductor_count = simulation->heat_conductors.count;
    next.port_count = simulation->heat_ports.count;
    if ((next.conductor_count != 0U
            && (next.conductors = factory_topology_calloc(simulation,
                FACTORY_TOPOLOGY_HEAT,next.conductor_count,
                sizeof(*next.conductors))) == NULL)
        || (next.port_count != 0U
            && (next.ports = factory_topology_calloc(simulation,
                FACTORY_TOPOLOGY_HEAT,next.port_count,
                sizeof(*next.ports))) == NULL)
        || (next.conductor_count != 0U
            && (next.networks = factory_topology_calloc(simulation,
                FACTORY_TOPOLOGY_HEAT,next.conductor_count,
                sizeof(*next.networks))) == NULL)) {
        factory_heat_network_state_destroy(&next);
        return FACTORY_RESULT_OUT_OF_MEMORY;
    }
    for (i=0U;i<next.conductor_count;++i) {
        const FactoryHeatConductor *c=&simulation->heat_conductors.items[i];
        next.conductors[i]=(FactoryHeatConductorInspection){
            c->entity_id,c->x,c->y,0U,c->entity_id,false};
    }
    if (next.conductor_count>1U) qsort(next.conductors,next.conductor_count,
        sizeof(*next.conductors),conductor_compare);
    for (i=0U;i<next.port_count;++i) {
        const FactoryHeatPort *p=&simulation->heat_ports.items[i];
        next.ports[i]=(FactoryHeatPortInspection){
            p->owner_entity_id,p->slot,FACTORY_HEAT_NETWORK_NONE,false};
    }
    if (next.port_count>1U) qsort(next.ports,next.port_count,
        sizeof(*next.ports),port_compare);
    {
        bool changed;
        do {
            changed=false;
            for (i=0U;i<next.conductor_count;++i)
                for (size_t j=i+1U;j<next.conductor_count;++j)
                    if (adjacent(next.conductors[i].x,next.conductors[i].y,
                            next.conductors[j].x,next.conductors[j].y)) {
                        FactoryHeatNetworkId m=next.conductors[i].network_id
                            < next.conductors[j].network_id
                            ? next.conductors[i].network_id
                            : next.conductors[j].network_id;
                        if (next.conductors[i].network_id!=m
                            || next.conductors[j].network_id!=m) {
                            next.conductors[i].network_id=m;
                            next.conductors[j].network_id=m; changed=true;
                        }
                    }
            for (size_t p=0U;p<simulation->heat_ports.count;++p) {
                const FactoryHeatPort *port=&simulation->heat_ports.items[p];
                FactoryHeatNetworkId m=UINT32_MAX;
                for (i=0U;i<next.conductor_count;++i)
                    if (adjacent(port->x,port->y,next.conductors[i].x,
                            next.conductors[i].y)
                        && next.conductors[i].network_id<m)
                        m=next.conductors[i].network_id;
                if (m==UINT32_MAX) continue;
                for (i=0U;i<next.conductor_count;++i)
                    if (adjacent(port->x,port->y,next.conductors[i].x,
                            next.conductors[i].y)
                        && next.conductors[i].network_id!=m) {
                        next.conductors[i].network_id=m; changed=true;
                    }
            }
        } while(changed);
    }
    for (i=0U;i<next.conductor_count;++i) {
        FactoryHeatConductorInspection *c=&next.conductors[i];
        FactoryHeatNetworkInspection *n=network_for(&next,c->network_id);
        if (n==NULL) {
            n=&next.networks[next.network_count++];
            n->network_id=c->network_id;
        }
        ++n->conductor_count;
        for (size_t j=0U;j<next.conductor_count;++j)
            if (i!=j && adjacent(c->x,c->y,next.conductors[j].x,
                    next.conductors[j].y))
                c->connection_mask |= connection_bit(
                    c->x,c->y,next.conductors[j].x,next.conductors[j].y);
        for (size_t p=0U;p<simulation->heat_ports.count;++p)
            if (adjacent(c->x,c->y,simulation->heat_ports.items[p].x,
                    simulation->heat_ports.items[p].y))
                c->connection_mask |= connection_bit(c->x,c->y,
                    simulation->heat_ports.items[p].x,
                    simulation->heat_ports.items[p].y);
        c->connected=c->connection_mask!=0U;
    }
    for (i=0U;i<next.port_count;++i) {
        FactoryHeatPortInspection *p=&next.ports[i];
        const FactoryHeatPort *component=NULL;
        for (size_t q=0U;q<simulation->heat_ports.count;++q)
            if (simulation->heat_ports.items[q].owner_entity_id
                    ==p->owner_entity_id
                && simulation->heat_ports.items[q].slot==p->slot)
                component=&simulation->heat_ports.items[q];
        if (component==NULL) continue;
        for (size_t j=0U;j<next.conductor_count;++j)
            if (adjacent(component->x,component->y,next.conductors[j].x,
                    next.conductors[j].y)
                && (p->network_id==0U
                    || next.conductors[j].network_id<p->network_id))
                p->network_id=next.conductors[j].network_id;
        p->connected=p->network_id!=0U;
        if (p->connected) {
            FactoryHeatNetworkInspection *n=network_for(&next,p->network_id);
            ++n->port_count;
            if (p->slot==FACTORY_HEAT_PORT_REACTOR_OUTPUT) ++n->source_count;
            else ++n->consumer_count;
        }
    }
    if (emit_events) {
        if (next.network_count>old->network_count)
            factory_simulation_emit_event(simulation,(FactoryEvent){
                .type=old->network_count==0U
                    ? FACTORY_EVENT_HEAT_NETWORK_CREATED
                    : FACTORY_EVENT_HEAT_NETWORK_SPLIT,
                .quantity=(uint32_t)(next.network_count-old->network_count)});
        else if (next.network_count<old->network_count)
            factory_simulation_emit_event(simulation,(FactoryEvent){
                .type=FACTORY_EVENT_HEAT_NETWORK_MERGED,
                .quantity=(uint32_t)(old->network_count-next.network_count)});
        for (i=0U;i<next.port_count;++i) {
            bool was=false;
            for (size_t j=0U;j<old->port_count;++j)
                if (old->ports[j].owner_entity_id==next.ports[i].owner_entity_id
                    && old->ports[j].slot==next.ports[i].slot)
                    was=old->ports[j].connected;
            if (was!=next.ports[i].connected)
                factory_simulation_emit_event(simulation,(FactoryEvent){
                    .type=next.ports[i].connected
                        ? FACTORY_EVENT_HEAT_PORT_CONNECTED
                        : FACTORY_EVENT_HEAT_PORT_DISCONNECTED,
                    .entity_id=next.ports[i].owner_entity_id,
                    .quantity=next.ports[i].slot});
        }
        for (i=0U;i<old->port_count;++i) {
            bool remains=false;
            if (!old->ports[i].connected) continue;
            for (size_t j=0U;j<next.port_count;++j)
                if (old->ports[i].owner_entity_id
                        ==next.ports[j].owner_entity_id
                    && old->ports[i].slot==next.ports[j].slot)
                    remains=true;
            if (!remains)
                factory_simulation_emit_event(simulation,(FactoryEvent){
                    .type=FACTORY_EVENT_HEAT_PORT_DISCONNECTED,
                    .entity_id=old->ports[i].owner_entity_id,
                    .quantity=old->ports[i].slot});
        }
    }
    factory_heat_network_state_destroy(old);
    simulation->heat_networks=next;
    return FACTORY_RESULT_OK;
}

static FactoryHeatPortInspection *port_for(
    FactoryHeatNetworkState *s, FactoryEntityId owner, FactoryHeatPortSlot slot)
{
    for (size_t i=0U;i<s->port_count;++i)
        if (s->ports[i].owner_entity_id==owner && s->ports[i].slot==slot)
            return &s->ports[i];
    return NULL;
}

void factory_heat_exchangers_update(FactorySimulation *simulation)
{
    const FactoryHeatExchangeRecipe *recipe=factory_content_heat_exchange_recipe_get(
        FACTORY_HEAT_EXCHANGE_RECIPE_WATER_TO_STEAM);
    for (size_t pi=0U;pi<simulation->heat_networks.port_count;++pi) {
        FactoryHeatPortInspection *consumer=&simulation->heat_networks.ports[pi];
        FactoryHeatExchanger *e;
        FactoryFluidStorage *water;
        FactoryFluidStorage *steam;
        FactoryHeatQuantity available=0U, needed=recipe->heat_input;
        FactoryHeatNetworkInspection *network;
        if (consumer->slot!=FACTORY_HEAT_PORT_EXCHANGER_INPUT) continue;
        e=factory_heat_exchanger_store_find_mutable(
            &simulation->heat_exchangers,consumer->owner_entity_id);
        if (e==NULL) continue;
        e->consumed_heat_last_tick=0U; e->consumed_water_last_tick=0U;
        e->produced_steam_last_tick=0U; e->activity=FACTORY_HEAT_EXCHANGER_IDLE;
        water=factory_fluid_storage_store_find_slot_mutable(
            &simulation->fluid_storages,e->entity_id,
            FACTORY_FLUID_STORAGE_HEAT_EXCHANGER_INPUT);
        steam=factory_fluid_storage_store_find_slot_mutable(
            &simulation->fluid_storages,e->entity_id,
            FACTORY_FLUID_STORAGE_HEAT_EXCHANGER_OUTPUT);
        if (!consumer->connected) {
            e->activity=FACTORY_HEAT_EXCHANGER_DISCONNECTED_HEAT; continue;
        }
        for (size_t si=0U;si<simulation->heat_networks.port_count;++si) {
            FactoryHeatPortInspection *source=&simulation->heat_networks.ports[si];
            const FactoryReactor *r;
            if (source->slot!=FACTORY_HEAT_PORT_REACTOR_OUTPUT
                || source->network_id!=consumer->network_id) continue;
            r=factory_reactor_store_find(&simulation->reactors,
                source->owner_entity_id);
            if (r!=NULL) available+=r->heat_storage.stored_heat;
            if (available>=needed) break;
        }
        if (available<needed) {
            e->activity=FACTORY_HEAT_EXCHANGER_BLOCKED_NO_HEAT; continue;
        }
        if (water==NULL || water->fluid_type!=FACTORY_FLUID_WATER
            || water->quantity<recipe->water_input) {
            e->activity=FACTORY_HEAT_EXCHANGER_BLOCKED_NO_WATER; continue;
        }
        if (steam==NULL
            || (steam->fluid_type!=FACTORY_FLUID_NONE
                && steam->fluid_type!=FACTORY_FLUID_STEAM)
            || steam->capacity-steam->quantity<recipe->steam_output) {
            e->activity=FACTORY_HEAT_EXCHANGER_BLOCKED_STEAM_FULL; continue;
        }
        for (size_t si=0U;si<simulation->heat_networks.port_count
                && needed!=0U;++si) {
            FactoryHeatPortInspection *source=&simulation->heat_networks.ports[si];
            FactoryReactor *r; FactoryHeatQuantity amount;
            if (source->slot!=FACTORY_HEAT_PORT_REACTOR_OUTPUT
                || source->network_id!=consumer->network_id) continue;
            r=factory_reactor_store_find_mutable(&simulation->reactors,
                source->owner_entity_id);
            if (r==NULL) continue;
            amount=r->heat_storage.stored_heat<needed
                ? r->heat_storage.stored_heat:needed;
            r->heat_storage.stored_heat-=amount; needed-=amount;
            if (amount!=0U)
                factory_simulation_emit_event(simulation,(FactoryEvent){
                    .type=FACTORY_EVENT_HEAT_TRANSFERRED,
                    .entity_id=r->entity_id,.related_entity_id=e->entity_id,
                    .quantity=(uint32_t)amount});
        }
        water->quantity-=recipe->water_input;
        if (water->quantity==0U) water->fluid_type=FACTORY_FLUID_NONE;
        steam->fluid_type=FACTORY_FLUID_STEAM;
        steam->quantity+=recipe->steam_output;
        e->consumed_heat_last_tick=recipe->heat_input;
        e->consumed_water_last_tick=recipe->water_input;
        e->produced_steam_last_tick=recipe->steam_output;
        e->activity=FACTORY_HEAT_EXCHANGER_WORKING;
        network=network_for(&simulation->heat_networks,consumer->network_id);
        network->transferred_last_tick+=recipe->heat_input;
        factory_simulation_emit_event(simulation,(FactoryEvent){
            .type=FACTORY_EVENT_HEAT_EXCHANGER_CYCLE_COMPLETED,
            .entity_id=e->entity_id,.fluid_type=FACTORY_FLUID_WATER,
            .related_fluid_type=FACTORY_FLUID_STEAM,
            .quantity=(uint32_t)recipe->heat_input,
            .related_quantity=recipe->water_input,
            .third_quantity=recipe->steam_output});
    }
}

FactoryResult factory_simulation_get_heat_conductor(
    const FactorySimulation *s, FactoryEntityId id,
    FactoryHeatConductorInspection *out)
{
    if (s==NULL || out==NULL || id==0U) return FACTORY_RESULT_INVALID_ARGUMENT;
    for (size_t i=0U;i<s->heat_networks.conductor_count;++i)
        if (s->heat_networks.conductors[i].entity_id==id) {
            *out=s->heat_networks.conductors[i]; return FACTORY_RESULT_OK;
        }
    return FACTORY_RESULT_ENTITY_NOT_FOUND;
}

FactoryResult factory_simulation_get_heat_port(
    const FactorySimulation *s, FactoryEntityId owner, FactoryHeatPortSlot slot,
    FactoryHeatPortInspection *out)
{
    FactoryHeatPortInspection *p;
    if (s==NULL || out==NULL || owner==0U) return FACTORY_RESULT_INVALID_ARGUMENT;
    p=port_for((FactoryHeatNetworkState *)&s->heat_networks,owner,slot);
    if (p==NULL) return FACTORY_RESULT_ENTITY_NOT_FOUND;
    *out=*p; return FACTORY_RESULT_OK;
}

size_t factory_simulation_get_heat_network_count(const FactorySimulation *s)
{ return s==NULL?0U:s->heat_networks.network_count; }

FactoryResult factory_simulation_get_heat_network(
    const FactorySimulation *s,size_t index,FactoryHeatNetworkInspection *out)
{
    if (s==NULL || out==NULL) return FACTORY_RESULT_INVALID_ARGUMENT;
    if (index>=s->heat_networks.network_count)
        return FACTORY_RESULT_HEAT_NETWORK_NOT_FOUND;
    *out=s->heat_networks.networks[index]; return FACTORY_RESULT_OK;
}

FactoryResult factory_simulation_get_heat_exchanger(
    const FactorySimulation *s,FactoryEntityId id,
    FactoryHeatExchangerInspection *out)
{
    const FactoryHeatExchanger *e;
    FactoryFluidStorageInspection water={0},steam={0};
    FactoryHeatPortInspection *p;
    if (s==NULL || out==NULL || id==0U) return FACTORY_RESULT_INVALID_ARGUMENT;
    e=factory_heat_exchanger_store_find(&s->heat_exchangers,id);
    if (e==NULL) return FACTORY_RESULT_ENTITY_NOT_FOUND;
    (void)factory_simulation_get_fluid_storage_slot(s,id,
        FACTORY_FLUID_STORAGE_HEAT_EXCHANGER_INPUT,&water);
    (void)factory_simulation_get_fluid_storage_slot(s,id,
        FACTORY_FLUID_STORAGE_HEAT_EXCHANGER_OUTPUT,&steam);
    p=port_for((FactoryHeatNetworkState *)&s->heat_networks,id,
        FACTORY_HEAT_PORT_EXCHANGER_INPUT);
    *out=(FactoryHeatExchangerInspection){
        id,p==NULL?0U:p->network_id,water.network_id,steam.network_id,
        water.quantity,water.capacity,steam.quantity,steam.capacity,
        e->consumed_heat_last_tick,e->consumed_water_last_tick,
        e->produced_steam_last_tick,e->activity};
    return FACTORY_RESULT_OK;
}
