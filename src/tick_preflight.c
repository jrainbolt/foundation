#include "tick_preflight_internal.h"

#include "simulation_internal.h"

#include <stdlib.h>
#include <string.h>

static size_t allocation_successes_before_failure=SIZE_MAX;

void factory_tick_preflight_test_fail_allocations_after(size_t successes)
{
    allocation_successes_before_failure=successes;
}

static void *preflight_calloc(size_t count,size_t width)
{
    if (allocation_successes_before_failure!=SIZE_MAX) {
        if (allocation_successes_before_failure==0U) return NULL;
        --allocation_successes_before_failure;
    }
    return calloc(count,width);
}

void factory_tick_preflight_destroy(FactoryTickPreflight *p)
{
    size_t i;
    if (p==NULL) return;
    for (i=0U;i<p->count;++i) free(p->blocks[i].pointer);
    *p=(FactoryTickPreflight){0};
}

static bool add_block(FactoryTickPreflight *p,FactoryTopologyDomain domain,
    size_t count,size_t width)
{
    void *pointer;
    if (count==0U) return true;
    if (count>SIZE_MAX/width || p->count>=FACTORY_TOPOLOGY_BLOCK_COUNT)
        return false;
    pointer=preflight_calloc(count,width);
    if (pointer==NULL) return false;
    p->blocks[p->count++]=(FactoryTopologyBlock){
        pointer,count*width,width,domain};
    return true;
}

static bool reserve_records(
    void **items,size_t *capacity,size_t required,size_t width)
{
    void *resized;
    if (required<=*capacity) return true;
    if (required>SIZE_MAX/width) return false;
    resized=realloc(*items,required*width);
    if (resized==NULL) return false;
    *items=resized; *capacity=required;
    return true;
}

static bool add_size(size_t *total,size_t value)
{
    if (*total>SIZE_MAX-value) return false;
    *total+=value;
    return true;
}

FactoryResult factory_simulation_preflight_tick(FactorySimulation *s)
{
    FactoryTickPreflight next={0};
    size_t q,additions=0U,poles,generators,accumulators,consumers,connections;
    size_t pipes,ports,conductors,heat_ports;
    size_t rail_count,switch_count,station_count,rail_nodes;
    if (s==NULL) return FACTORY_RESULT_INVALID_ARGUMENT;
    q=s->command_count;
    for (size_t i=0U;i<q;++i) switch (s->commands[i].type) {
        case FACTORY_COMMAND_SET_REFINERY_RECIPE:
        case FACTORY_COMMAND_DEMOLISH_ENTITY:
        case FACTORY_COMMAND_GRANT_CONSTRUCTION_UNITS:
        case FACTORY_COMMAND_SET_ASSEMBLER_RECIPE:
        case FACTORY_COMMAND_SET_STORAGE_OUTPUT:
        case FACTORY_COMMAND_FLUID_INSERT:
        case FACTORY_COMMAND_FLUID_REMOVE:
        case FACTORY_COMMAND_FLUID_TRANSFER:
        case FACTORY_COMMAND_INSERT_REACTOR_FUEL:
        case FACTORY_COMMAND_SELECT_RESEARCH:
        case FACTORY_COMMAND_SET_RAIL_SWITCH_BRANCH:
        case FACTORY_COMMAND_COUPLE_REAR_WAGON:
        case FACTORY_COMMAND_DECOUPLE_REAR_WAGON:
        case FACTORY_COMMAND_SET_TRAIN_DESTINATION:
        case FACTORY_COMMAND_CLEAR_TRAIN_DESTINATION:
        case FACTORY_COMMAND_REPLAN_TRAIN_ROUTE:
            break;
        default:
            ++additions;
            break;
    }
    q=additions;
#define ADD_BOUND(out,current,multiplier) do {                              \
    if ((current)>SIZE_MAX-q*(multiplier)) goto overflow;                   \
    (out)=(current)+q*(multiplier);                                         \
} while (false)
    if (q!=0U && q>SIZE_MAX/2U) goto overflow;
#define RESERVE_STORE(store,multiplier) do {                                \
    size_t required;                                                        \
    if ((store).count>SIZE_MAX-q*(multiplier)) goto overflow;               \
    required=(store).count+q*(multiplier);                                  \
    if (!reserve_records((void **)&(store).items,&(store).capacity,required, \
            sizeof(*(store).items))) goto allocation_failed;               \
} while (false)
    if (s->entities->count>SIZE_MAX-q) goto overflow;
    if (!reserve_records((void **)&s->entities->live_ids,
            &s->entities->capacity,s->entities->count+q,
            sizeof(*s->entities->live_ids))) goto allocation_failed;
    RESERVE_STORE(s->extractors,1U);
    RESERVE_STORE(s->refineries,1U);
    RESERVE_STORE(s->assemblers,1U);
    RESERVE_STORE(s->splitters,1U);
    RESERVE_STORE(s->inserters,1U);
    RESERVE_STORE(s->belts,1U);
    RESERVE_STORE(s->storages,1U);
    RESERVE_STORE(s->power_poles,1U);
    RESERVE_STORE(s->power_generators,1U);
    RESERVE_STORE(s->burners,1U);
    RESERVE_STORE(s->fluid_storages,2U);
    RESERVE_STORE(s->pipes,1U);
    RESERVE_STORE(s->fluid_ports,2U);
    RESERVE_STORE(s->water_extractors,1U);
    RESERVE_STORE(s->boilers,1U);
    RESERVE_STORE(s->steam_engines,1U);
    RESERVE_STORE(s->steam_turbines,1U);
    RESERVE_STORE(s->steam_condensers,1U);
    RESERVE_STORE(s->solar_generators,1U);
    RESERVE_STORE(s->accumulators,1U);
    RESERVE_STORE(s->reactors,1U);
    RESERVE_STORE(s->heat_conductors,1U);
    RESERVE_STORE(s->heat_ports,1U);
    RESERVE_STORE(s->heat_exchangers,1U);
    RESERVE_STORE(s->research_labs,1U);
    RESERVE_STORE(s->construction_depots,1U);
    RESERVE_STORE(s->rails,1U);
    RESERVE_STORE(s->rail_stations,1U);
    RESERVE_STORE(s->rail_switches,1U);
    RESERVE_STORE(s->rail_signals,1U);
    RESERVE_STORE(s->rail_chain_signals,1U);
    {
        size_t required;
        if(s->locomotives.count>SIZE_MAX-q)goto overflow;
        required=s->locomotives.count+q;
        if(!factory_locomotive_store_reserve(&s->locomotives,required))
            goto allocation_failed;
    }
    {
        size_t required;
        if(s->cargo_wagons.count>SIZE_MAX-q)goto overflow;
        required=s->cargo_wagons.count+q;
        if(!factory_cargo_wagon_store_reserve(&s->cargo_wagons,required))
            goto allocation_failed;
    }
#undef RESERVE_STORE
    ADD_BOUND(poles,s->power_poles.count,1U);
    ADD_BOUND(generators,s->power_generators.count,1U);
    ADD_BOUND(accumulators,s->accumulators.count,1U);
    consumers=0U;
    if (!add_size(&consumers,s->extractors.count)
        || !add_size(&consumers,s->refineries.count)
        || !add_size(&consumers,s->assemblers.count)
        || !add_size(&consumers,s->inserters.count)
        || !add_size(&consumers,s->steam_condensers.count)
        || !add_size(&consumers,s->research_labs.count)
        || !add_size(&consumers,q)) goto overflow;
    connections=0U;
    if (poles>1U) {
        if (poles>SIZE_MAX/(poles-1U)) goto overflow;
        connections=poles*(poles-1U)/2U;
    }
    ADD_BOUND(pipes,s->pipes.count,1U);
    ADD_BOUND(ports,s->fluid_ports.count,2U);
    ADD_BOUND(conductors,s->heat_conductors.count,1U);
    ADD_BOUND(heat_ports,s->heat_ports.count,1U);
    ADD_BOUND(rail_count,s->rails.count,1U);
    ADD_BOUND(switch_count,s->rail_switches.count,1U);
    ADD_BOUND(station_count,s->rail_stations.count,1U);
    rail_nodes=rail_count;
    if (!add_size(&rail_nodes,switch_count)) goto overflow;
    if (!add_block(&next,FACTORY_TOPOLOGY_POWER,poles,
            sizeof(FactoryPowerPoleInspection))
        || !add_block(&next,FACTORY_TOPOLOGY_POWER,generators,
            sizeof(FactoryPowerGeneratorInspection))
        || !add_block(&next,FACTORY_TOPOLOGY_POWER,accumulators,
            sizeof(FactoryPowerAccumulatorInspection))
        || !add_block(&next,FACTORY_TOPOLOGY_POWER,consumers,
            sizeof(FactoryPowerConsumerInspection))
        || !add_block(&next,FACTORY_TOPOLOGY_POWER,connections,
            sizeof(FactoryPowerConnectionInspection))
        || !add_block(&next,FACTORY_TOPOLOGY_POWER,poles,
            sizeof(FactoryPowerNetworkInspection))
        || !add_block(&next,FACTORY_TOPOLOGY_POWER,generators,
            sizeof(FactoryPowerUnits))
        || !add_block(&next,FACTORY_TOPOLOGY_POWER,generators,
            sizeof(FactoryPowerUnits))
        || !add_block(&next,FACTORY_TOPOLOGY_POWER,generators,sizeof(bool))
        || !add_block(&next,FACTORY_TOPOLOGY_POWER,generators,
            sizeof(FactoryPowerUnits))
        || !add_block(&next,FACTORY_TOPOLOGY_FLUID,pipes,
            sizeof(FactoryPipeInspection))
        || !add_block(&next,FACTORY_TOPOLOGY_FLUID,ports,
            sizeof(FactoryFluidPortInspection))
        || !add_block(&next,FACTORY_TOPOLOGY_FLUID,pipes,
            sizeof(FactoryFluidNetworkInspection))
        || !add_block(&next,FACTORY_TOPOLOGY_HEAT,conductors,
            sizeof(FactoryHeatConductorInspection))
        || !add_block(&next,FACTORY_TOPOLOGY_HEAT,heat_ports,
            sizeof(FactoryHeatPortInspection))
        || !add_block(&next,FACTORY_TOPOLOGY_HEAT,conductors,
            sizeof(FactoryHeatNetworkInspection))
        || !add_block(&next,FACTORY_TOPOLOGY_RAIL,rail_count,
            sizeof(FactoryRailInspection))
        || !add_block(&next,FACTORY_TOPOLOGY_RAIL,switch_count,
            sizeof(FactoryRailSwitchInspection))
        || !add_block(&next,FACTORY_TOPOLOGY_RAIL,station_count,
            sizeof(FactoryRailStationInspection))
        || !add_block(&next,FACTORY_TOPOLOGY_RAIL,rail_nodes,
            sizeof(FactoryRailNetworkInspection))
        || !add_block(&next,FACTORY_TOPOLOGY_RAIL,rail_nodes,
            sizeof(FactoryRailBlock))
        || !add_block(&next,FACTORY_TOPOLOGY_RAIL,rail_nodes,
            sizeof(FactoryRailBlockMember))) {
        factory_tick_preflight_destroy(&next);
        return FACTORY_RESULT_OUT_OF_MEMORY;
    }
#undef ADD_BOUND
    factory_tick_preflight_destroy(&s->tick_preflight);
    next.active=true;
    s->tick_preflight=next;
    return FACTORY_RESULT_OK;
overflow:
#undef ADD_BOUND
#undef RESERVE_STORE
    factory_tick_preflight_destroy(&next);
    return FACTORY_RESULT_OUT_OF_MEMORY;
allocation_failed:
#undef ADD_BOUND
#undef RESERVE_STORE
    factory_tick_preflight_destroy(&next);
    return FACTORY_RESULT_OUT_OF_MEMORY;
}

void *factory_topology_calloc(FactorySimulation *s,
    FactoryTopologyDomain domain,size_t count,size_t width)
{
    FactoryTickPreflight *p;
    size_t i,bytes;
    if (count==0U) return NULL;
    if (count>SIZE_MAX/width) return NULL;
    if (s==NULL || !s->tick_preflight.active) return calloc(count,width);
    p=&s->tick_preflight; bytes=count*width;
    for (i=p->cursor[domain];i<p->count;++i) {
        FactoryTopologyBlock *block=&p->blocks[i];
        if (block->domain!=domain || block->pointer==NULL
            || block->width!=width) continue;
        if (block->bytes<bytes) return NULL;
        p->cursor[domain]=i+1U;
        {
            void *result=block->pointer;
            block->pointer=NULL;
            (void)memset(result,0,block->bytes);
            return result;
        }
    }
    return NULL;
}

void factory_tick_preflight_finish(FactorySimulation *s)
{
    if (s!=NULL) factory_tick_preflight_destroy(&s->tick_preflight);
}
