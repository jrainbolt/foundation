#include "rail_internal.h"
#include "simulation_internal.h"
#include "tick_preflight_internal.h"

#include <stdlib.h>

static const int32_t direction_x[4]={0,1,0,-1};
static const int32_t direction_y[4]={-1,0,1,0};

uint32_t factory_rail_geometry_port_mask(FactoryRailGeometry geometry)
{
    static const uint32_t masks[FACTORY_RAIL_GEOMETRY_COUNT]={
        FACTORY_RAIL_PORT_EAST|FACTORY_RAIL_PORT_WEST,
        FACTORY_RAIL_PORT_NORTH|FACTORY_RAIL_PORT_SOUTH,
        FACTORY_RAIL_PORT_NORTH|FACTORY_RAIL_PORT_EAST,
        FACTORY_RAIL_PORT_NORTH|FACTORY_RAIL_PORT_WEST,
        FACTORY_RAIL_PORT_SOUTH|FACTORY_RAIL_PORT_EAST,
        FACTORY_RAIL_PORT_SOUTH|FACTORY_RAIL_PORT_WEST};
    return factory_rail_geometry_is_valid(geometry)?masks[geometry]:0U;
}

bool factory_rail_geometry_is_valid(FactoryRailGeometry geometry)
{
    return geometry>=FACTORY_RAIL_HORIZONTAL
        &&geometry<FACTORY_RAIL_GEOMETRY_COUNT;
}

bool factory_rail_switch_geometry_is_valid(FactoryRailSwitchGeometry geometry)
{
    return geometry>=FACTORY_RAIL_SWITCH_STEM_NORTH
        &&geometry<FACTORY_RAIL_SWITCH_GEOMETRY_COUNT;
}

bool factory_rail_switch_branch_is_valid(FactoryRailSwitchBranch branch)
{
    return branch>=FACTORY_RAIL_SWITCH_BRANCH_A
        &&branch<FACTORY_RAIL_SWITCH_BRANCH_COUNT;
}

bool factory_rail_switch_geometry_directions(FactoryRailSwitchGeometry geometry,
    FactoryDirection *out_stem,FactoryDirection *out_branch_a,
    FactoryDirection *out_branch_b)
{
    static const FactoryDirection directions[FACTORY_RAIL_SWITCH_GEOMETRY_COUNT][3]={
        {FACTORY_DIRECTION_NORTH,FACTORY_DIRECTION_EAST,FACTORY_DIRECTION_WEST},
        {FACTORY_DIRECTION_SOUTH,FACTORY_DIRECTION_EAST,FACTORY_DIRECTION_WEST},
        {FACTORY_DIRECTION_EAST,FACTORY_DIRECTION_NORTH,FACTORY_DIRECTION_SOUTH},
        {FACTORY_DIRECTION_WEST,FACTORY_DIRECTION_NORTH,FACTORY_DIRECTION_SOUTH}};
    if(!factory_rail_switch_geometry_is_valid(geometry)||out_stem==NULL
        ||out_branch_a==NULL||out_branch_b==NULL)return false;
    *out_stem=directions[geometry][0];
    *out_branch_a=directions[geometry][1];
    *out_branch_b=directions[geometry][2];
    return true;
}

uint32_t factory_rail_switch_geometry_port_mask(
    FactoryRailSwitchGeometry geometry)
{
    FactoryDirection stem,branch_a,branch_b;
    if(!factory_rail_switch_geometry_directions(geometry,&stem,&branch_a,
            &branch_b))return 0U;
    return (UINT32_C(1)<<(uint32_t)stem)
        |(UINT32_C(1)<<(uint32_t)branch_a)
        |(UINT32_C(1)<<(uint32_t)branch_b);
}

#define STORE_FUNCTIONS(prefix,Store,Item) \
void prefix##_destroy(Store*s){if(s!=NULL){free(s->items);*s=(Store){0};}} \
bool prefix##_reserve_one(Store*s){Item*p;size_t capacity;if(s==NULL)return false;if(s->count<s->capacity)return true;capacity=s->capacity==0U?4U:s->capacity*2U;if(capacity<s->capacity||capacity>SIZE_MAX/sizeof(*p))return false;p=realloc(s->items,capacity*sizeof(*p));if(p==NULL)return false;s->items=p;s->capacity=capacity;return true;} \
bool prefix##_remove(Store*s,FactoryEntityId id){if(s==NULL)return false;for(size_t i=0U;i<s->count;++i)if(s->items[i].entity_id==id){--s->count;s->items[i]=s->items[s->count];return true;}return false;}
STORE_FUNCTIONS(factory_rail_store,FactoryRailStore,FactoryRail)
STORE_FUNCTIONS(factory_rail_station_store,FactoryRailStationStore,FactoryRailStation)
STORE_FUNCTIONS(factory_rail_switch_store,FactoryRailSwitchStore,FactoryRailSwitch)
#undef STORE_FUNCTIONS

void factory_rail_store_add(FactoryRailStore*s,FactoryEntityId id,int32_t x,
    int32_t y,FactoryRailGeometry geometry)
{s->items[s->count++]=(FactoryRail){id,x,y,geometry};}

const FactoryRail *factory_rail_store_find(const FactoryRailStore*s,
    FactoryEntityId id)
{if(s!=NULL)for(size_t i=0U;i<s->count;++i)if(s->items[i].entity_id==id)return &s->items[i];return NULL;}

void factory_rail_station_store_add(FactoryRailStationStore*s,
    FactoryEntityId id,int32_t x,int32_t y,FactoryDirection orientation)
{s->items[s->count++]=(FactoryRailStation){id,x,y,orientation};}

const FactoryRailStation *factory_rail_station_store_find(
    const FactoryRailStationStore*s,FactoryEntityId id)
{if(s!=NULL)for(size_t i=0U;i<s->count;++i)if(s->items[i].entity_id==id)return &s->items[i];return NULL;}

void factory_rail_switch_store_add(FactoryRailSwitchStore*s,
    FactoryEntityId id,int32_t x,int32_t y,FactoryRailSwitchGeometry geometry)
{s->items[s->count++]=(FactoryRailSwitch){id,x,y,geometry,
    FACTORY_RAIL_SWITCH_BRANCH_A};}

const FactoryRailSwitch *factory_rail_switch_store_find(
    const FactoryRailSwitchStore*s,FactoryEntityId id)
{if(s!=NULL)for(size_t i=0U;i<s->count;++i)if(s->items[i].entity_id==id)return &s->items[i];return NULL;}

FactoryRailSwitch *factory_rail_switch_store_find_mutable(
    FactoryRailSwitchStore*s,FactoryEntityId id)
{if(s!=NULL)for(size_t i=0U;i<s->count;++i)if(s->items[i].entity_id==id)return &s->items[i];return NULL;}

void factory_rail_topology_destroy(FactoryRailTopology*topology)
{
    if(topology==NULL)return;
    free(topology->rails);free(topology->switches);free(topology->stations);
    free(topology->networks);*topology=(FactoryRailTopology){0};
}

static int rail_compare(const void*a,const void*b)
{FactoryEntityId x=((const FactoryRailInspection*)a)->entity_id,y=((const FactoryRailInspection*)b)->entity_id;return x<y?-1:x>y;}
static int switch_compare(const void*a,const void*b)
{FactoryEntityId x=((const FactoryRailSwitchInspection*)a)->entity_id,y=((const FactoryRailSwitchInspection*)b)->entity_id;return x<y?-1:x>y;}
static uint32_t opposite(uint32_t port)
{return ((port<<2U)|(port>>2U))&UINT32_C(0x0f);}

typedef struct {
    FactoryEntityId *entity_id;
    int32_t x,y;
    uint32_t port_mask;
    uint32_t *connection_mask;
    FactoryEntityId *neighbors;
    FactoryRailNetworkId *network_id;
    uint32_t *connection_count;
    bool is_switch;
} Node;

static size_t node_count(const FactoryRailTopology*t)
{return t->rail_count+t->switch_count;}

static Node node_at_index(FactoryRailTopology*t,size_t index)
{
    if(index<t->rail_count){FactoryRailInspection*r=&t->rails[index];return (Node){
        &r->entity_id,r->x,r->y,r->port_mask,&r->connection_mask,r->neighbors,
        &r->network_id,&r->connection_count,false};}
    FactoryRailSwitchInspection*s=&t->switches[index-t->rail_count];return (Node){
        &s->entity_id,s->x,s->y,s->port_mask,&s->connection_mask,s->neighbors,
        &s->network_id,&s->connection_count,true};
}

static bool find_node_at(FactoryRailTopology*t,int32_t x,int32_t y,Node*out)
{
    for(size_t i=0U;i<node_count(t);++i){Node node=node_at_index(t,i);
        if(node.x==x&&node.y==y){*out=node;return true;}}
    return false;
}

static bool find_node_by_id(FactoryRailTopology*t,FactoryEntityId id,Node*out)
{
    for(size_t i=0U;i<node_count(t);++i){Node node=node_at_index(t,i);
        if(*node.entity_id==id){*out=node;return true;}}
    return false;
}

static FactoryRailNetworkInspection *network_at(FactoryRailTopology*t,
    FactoryRailNetworkId id)
{for(size_t i=0U;i<t->network_count;++i)if(t->networks[i].network_id==id)return &t->networks[i];return NULL;}

FactoryResult factory_rail_topology_rebuild(FactorySimulation*simulation)
{
    FactoryRailTopology next={0};
    next.rail_count=simulation->rails.count;
    next.switch_count=simulation->rail_switches.count;
    next.station_count=simulation->rail_stations.count;
    if(next.rail_count>SIZE_MAX-next.switch_count)
        return FACTORY_RESULT_OUT_OF_MEMORY;
    size_t nodes=node_count(&next);
    if((next.rail_count!=0U&&(next.rails=factory_topology_calloc(simulation,
            FACTORY_TOPOLOGY_RAIL,next.rail_count,sizeof(*next.rails)))==NULL)
        ||(next.switch_count!=0U&&(next.switches=factory_topology_calloc(simulation,
            FACTORY_TOPOLOGY_RAIL,next.switch_count,sizeof(*next.switches)))==NULL)
        ||(next.station_count!=0U&&(next.stations=factory_topology_calloc(simulation,
            FACTORY_TOPOLOGY_RAIL,next.station_count,sizeof(*next.stations)))==NULL)
        ||(nodes!=0U&&(next.networks=factory_topology_calloc(simulation,
            FACTORY_TOPOLOGY_RAIL,nodes,sizeof(*next.networks)))==NULL)){
        factory_rail_topology_destroy(&next);return FACTORY_RESULT_OUT_OF_MEMORY;
    }
    for(size_t i=0U;i<next.rail_count;++i){const FactoryRail*r=&simulation->rails.items[i];
        next.rails[i]=(FactoryRailInspection){.entity_id=r->entity_id,.x=r->x,
            .y=r->y,.geometry=r->geometry,
            .port_mask=factory_rail_geometry_port_mask(r->geometry),
            .network_id=r->entity_id};}
    for(size_t i=0U;i<next.switch_count;++i){const FactoryRailSwitch*s=&simulation->rail_switches.items[i];
        FactoryDirection stem,branch_a,branch_b;
        (void)factory_rail_switch_geometry_directions(s->geometry,&stem,&branch_a,&branch_b);
        next.switches[i]=(FactoryRailSwitchInspection){.entity_id=s->entity_id,
            .x=s->x,.y=s->y,.geometry=s->geometry,
            .selected_branch=s->selected_branch,.stem_direction=stem,
            .branch_a_direction=branch_a,.branch_b_direction=branch_b,
            .port_mask=factory_rail_switch_geometry_port_mask(s->geometry),
            .network_id=s->entity_id};}
    if(next.rail_count>1U)qsort(next.rails,next.rail_count,sizeof(*next.rails),rail_compare);
    if(next.switch_count>1U)qsort(next.switches,next.switch_count,sizeof(*next.switches),switch_compare);
    for(size_t i=0U;i<nodes;++i){Node node=node_at_index(&next,i);
        for(size_t direction=0U;direction<4U;++direction){uint32_t port=UINT32_C(1)<<direction;Node neighbor;
            if((node.port_mask&port)!=0U&&find_node_at(&next,
                    node.x+direction_x[direction],node.y+direction_y[direction],
                    &neighbor)&&(neighbor.port_mask&opposite(port))!=0U){
                node.neighbors[direction]=*neighbor.entity_id;
                *node.connection_mask|=port;++*node.connection_count;
            }
        }
    }
    {bool changed;do{changed=false;for(size_t i=0U;i<nodes;++i){Node node=node_at_index(&next,i);
        for(size_t direction=0U;direction<4U;++direction)if(node.neighbors[direction]!=0U){Node neighbor;
            if(find_node_by_id(&next,node.neighbors[direction],&neighbor)){
                FactoryRailNetworkId minimum=*node.network_id<*neighbor.network_id
                    ?*node.network_id:*neighbor.network_id;
                if(*node.network_id!=minimum||*neighbor.network_id!=minimum){
                    *node.network_id=minimum;*neighbor.network_id=minimum;changed=true;
                }
            }
        }
    }}while(changed);}
    while(next.network_count<nodes){FactoryRailNetworkId minimum=UINT32_MAX;
        for(size_t i=0U;i<nodes;++i){Node node=node_at_index(&next,i);
            if(network_at(&next,*node.network_id)==NULL&&*node.network_id<minimum)
                minimum=*node.network_id;}
        if(minimum==UINT32_MAX)break;
        FactoryRailNetworkInspection*network=&next.networks[next.network_count++];
        network->network_id=minimum;
        for(size_t i=0U;i<nodes;++i){Node node=node_at_index(&next,i);
            if(*node.network_id==minimum){if(node.is_switch)++network->switch_count;
                else ++network->rail_count;}}
    }
    for(size_t i=0U;i<next.station_count;++i){const FactoryRailStation*station=&simulation->rail_stations.items[i];Node attached;
        bool connected=find_node_at(&next,
            station->x+direction_x[station->orientation],
            station->y+direction_y[station->orientation],&attached);
        next.stations[i]=(FactoryRailStationInspection){.entity_id=station->entity_id,
            .x=station->x,.y=station->y,.orientation=station->orientation,
            .attached_rail_id=connected?*attached.entity_id:0U,
            .network_id=connected?*attached.network_id:0U,.connected=connected};
        if(connected)++network_at(&next,*attached.network_id)->station_count;
    }
    factory_rail_topology_destroy(&simulation->rail_topology);
    simulation->rail_topology=next;return FACTORY_RESULT_OK;
}

bool factory_simulation_get_rail(const FactorySimulation*s,FactoryEntityId id,
    FactoryRailInspection*out)
{if(s==NULL||out==NULL)return false;for(size_t i=0U;i<s->rail_topology.rail_count;++i)if(s->rail_topology.rails[i].entity_id==id){*out=s->rail_topology.rails[i];return true;}return false;}

bool factory_simulation_get_rail_switch(const FactorySimulation*s,
    FactoryEntityId id,FactoryRailSwitchInspection*out)
{if(s==NULL||out==NULL)return false;for(size_t i=0U;i<s->rail_topology.switch_count;++i)if(s->rail_topology.switches[i].entity_id==id){*out=s->rail_topology.switches[i];return true;}return false;}

bool factory_simulation_get_rail_station(const FactorySimulation*s,
    FactoryEntityId id,FactoryRailStationInspection*out)
{if(s==NULL||out==NULL)return false;for(size_t i=0U;i<s->rail_topology.station_count;++i)if(s->rail_topology.stations[i].entity_id==id){*out=s->rail_topology.stations[i];return true;}return false;}

bool factory_simulation_get_rail_traversal(const FactorySimulation*s,
    FactoryEntityId id,FactoryDirection entry,FactoryRailTraversal*out)
{
    if(out!=NULL)*out=(FactoryRailTraversal){0};
    if(s==NULL||out==NULL||entry<FACTORY_DIRECTION_NORTH
        ||entry>FACTORY_DIRECTION_WEST)return false;
    FactoryRailInspection rail;
    if(factory_simulation_get_rail(s,id,&rail)){
        uint32_t entry_port=UINT32_C(1)<<(uint32_t)entry;
        if((rail.port_mask&entry_port)==0U||rail.neighbors[entry]==0U)return true;
        for(size_t direction=0U;direction<4U;++direction)
            if(direction!=(size_t)entry&&rail.neighbors[direction]!=0U
                &&(rail.port_mask&(UINT32_C(1)<<direction))!=0U){
                *out=(FactoryRailTraversal){true,(FactoryDirection)direction,
                    rail.neighbors[direction]};return true;}
        return true;
    }
    FactoryRailSwitchInspection rail_switch;
    if(!factory_simulation_get_rail_switch(s,id,&rail_switch))return false;
    FactoryDirection selected=rail_switch.selected_branch==FACTORY_RAIL_SWITCH_BRANCH_A
        ?rail_switch.branch_a_direction:rail_switch.branch_b_direction;
    FactoryDirection exit;
    if(entry==rail_switch.stem_direction)exit=selected;
    else if(entry==selected)exit=rail_switch.stem_direction;
    else return true;
    if(rail_switch.neighbors[entry]!=0U&&rail_switch.neighbors[exit]!=0U)
        *out=(FactoryRailTraversal){true,exit,rail_switch.neighbors[exit]};
    return true;
}

size_t factory_simulation_get_rail_network_count(const FactorySimulation*s)
{return s==NULL?0U:s->rail_topology.network_count;}

const FactoryRailNetworkInspection *factory_simulation_get_rail_network(
    const FactorySimulation*s,size_t index)
{return s!=NULL&&index<s->rail_topology.network_count?s->rail_topology.networks+index:NULL;}

void factory_locomotive_store_destroy(FactoryLocomotiveStore*s)
{if(s!=NULL){free(s->items);free(s->plans);*s=(FactoryLocomotiveStore){0};}}

bool factory_locomotive_store_reserve(FactoryLocomotiveStore*s,size_t required)
{
    if(s==NULL||required>SIZE_MAX/sizeof(*s->items)
        ||required>SIZE_MAX/sizeof(*s->plans))return false;
    if(required>s->capacity){FactoryLocomotive*p=realloc(s->items,
        required*sizeof(*p));if(p==NULL)return false;s->items=p;s->capacity=required;}
    if(required>s->plan_capacity){FactoryLocomotivePlan*p=realloc(s->plans,
        required*sizeof(*p));if(p==NULL)return false;s->plans=p;s->plan_capacity=required;}
    return true;
}

const FactoryLocomotive *factory_locomotive_store_find(
    const FactoryLocomotiveStore*s,FactoryEntityId id)
{if(s!=NULL)for(size_t i=0;i<s->count;++i)if(s->items[i].entity_id==id)return s->items+i;return NULL;}
FactoryLocomotive *factory_locomotive_store_find_mutable(
    FactoryLocomotiveStore*s,FactoryEntityId id)
{return (FactoryLocomotive*)factory_locomotive_store_find(s,id);}
bool factory_locomotive_store_remove(FactoryLocomotiveStore*s,FactoryEntityId id)
{if(s!=NULL)for(size_t i=0;i<s->count;++i)if(s->items[i].entity_id==id){--s->count;s->items[i]=s->items[s->count];return true;}return false;}

void factory_cargo_wagon_store_destroy(FactoryCargoWagonStore*s)
{if(s!=NULL){free(s->items);*s=(FactoryCargoWagonStore){0};}}
bool factory_cargo_wagon_store_reserve(FactoryCargoWagonStore*s,size_t required)
{
    if(s==NULL||required>SIZE_MAX/sizeof(*s->items))return false;
    if(required>s->capacity){
        FactoryCargoWagon*p=realloc(s->items,required*sizeof(*p));
        if(p==NULL)return false;
        s->items=p;
        s->capacity=required;
    }
    return true;
}
const FactoryCargoWagon *factory_cargo_wagon_store_find(
    const FactoryCargoWagonStore*s,FactoryEntityId id)
{if(s!=NULL)for(size_t i=0;i<s->count;++i)if(s->items[i].entity_id==id)return s->items+i;return NULL;}
FactoryCargoWagon *factory_cargo_wagon_store_find_mutable(
    FactoryCargoWagonStore*s,FactoryEntityId id)
{return (FactoryCargoWagon*)factory_cargo_wagon_store_find(s,id);}
bool factory_cargo_wagon_store_remove(FactoryCargoWagonStore*s,FactoryEntityId id)
{if(s!=NULL)for(size_t i=0;i<s->count;++i)if(s->items[i].entity_id==id){--s->count;s->items[i]=s->items[s->count];return true;}return false;}

FactoryEntityId factory_simulation_get_rail_vehicle_occupant(
    const FactorySimulation*s,FactoryEntityId rail_id)
{
    if(s==NULL)return 0U;
    for(size_t i=0U;i<s->locomotives.count;++i){
        if(s->locomotives.items[i].rail_entity_id==rail_id)
            return s->locomotives.items[i].entity_id;
    }
    for(size_t i=0U;i<s->cargo_wagons.count;++i){
        if(s->cargo_wagons.items[i].rail_entity_id==rail_id)
            return s->cargo_wagons.items[i].entity_id;
    }
    return 0U;
}

static bool rail_node_position(const FactorySimulation*s,FactoryEntityId id,
    int32_t*x,int32_t*y,FactoryRailNetworkId*network)
{FactoryRailInspection r;FactoryRailSwitchInspection w;
 if(factory_simulation_get_rail(s,id,&r)){*x=r.x;*y=r.y;*network=r.network_id;return true;}
 if(factory_simulation_get_rail_switch(s,id,&w)){*x=w.x;*y=w.y;*network=w.network_id;return true;}return false;}

bool factory_simulation_get_locomotive(const FactorySimulation*s,
    FactoryEntityId id,FactoryLocomotiveInspection*out)
{
    if(s==NULL||out==NULL)return false;
    const FactoryLocomotive*l=
        factory_locomotive_store_find(&s->locomotives,id);
    if(l==NULL)return false;
    int32_t x=0,y=0;FactoryRailNetworkId network=0;FactoryRailTraversal t={0};
    bool exists=rail_node_position(s,l->rail_entity_id,&x,&y,&network);
    bool traversed=exists&&factory_simulation_get_rail_traversal(s,
        l->rail_entity_id,l->entry_direction,&t)&&t.allowed;
    FactoryLocomotiveActivity activity=l->activity;
    if(l->progress==FACTORY_LOCOMOTIVE_MOVE_TICKS){
        if(!exists)activity=FACTORY_LOCOMOTIVE_DISCONNECTED;
        else if(!traversed)activity=factory_rail_switch_store_find(
            &s->rail_switches,l->rail_entity_id)!=NULL
                ?FACTORY_LOCOMOTIVE_BLOCKED_SWITCH:FACTORY_LOCOMOTIVE_BLOCKED_TRACK;
        else {FactoryEntityId occupant=factory_simulation_get_rail_vehicle_occupant(
            s,t.exit_entity_id);if(occupant!=0U&&occupant!=l->entity_id)
                activity=FACTORY_LOCOMOTIVE_BLOCKED_OCCUPIED;}
    }
    *out=(FactoryLocomotiveInspection){l->entity_id,l->rail_entity_id,x,y,
        l->entry_direction,traversed?t.exit_direction:l->entry_direction,
        l->progress,FACTORY_LOCOMOTIVE_MOVE_TICKS,traversed?t.exit_entity_id:0U,
        network,activity,l->entity_id,l->vehicle_count};return true;
}

bool factory_simulation_get_cargo_wagon(const FactorySimulation*s,
    FactoryEntityId id,FactoryCargoWagonInspection*out)
{
    if(s==NULL||out==NULL)return false;
    const FactoryCargoWagon*w=
        factory_cargo_wagon_store_find(&s->cargo_wagons,id);
    if(w==NULL)return false;
    int32_t x=0,y=0;FactoryRailNetworkId network=0;uint32_t index=0U;
    (void)rail_node_position(s,w->rail_entity_id,&x,&y,&network);
    if(w->train_id!=0U){const FactoryLocomotive*l=factory_locomotive_store_find(
        &s->locomotives,w->train_id);FactoryEntityId next=l!=NULL?l->rear_vehicle_id:0U;
        index=1U;while(next!=0U&&next!=id){const FactoryCargoWagon*cursor=
            factory_cargo_wagon_store_find(&s->cargo_wagons,next);
            if(cursor==NULL)break;
            next=cursor->next_vehicle_id;
            ++index;
        }
    }
    *out=(FactoryCargoWagonInspection){w->entity_id,w->rail_entity_id,x,y,
        w->entry_direction,network,w->train_id,index,w->train_id!=0U,
        w->cargo_item,w->cargo_quantity,FACTORY_CARGO_WAGON_CAPACITY};return true;
}

FactoryResult factory_simulation_cargo_wagon_insert(FactorySimulation*s,
    FactoryEntityId id,FactoryItemType item,uint32_t quantity)
{FactoryCargoWagon*w;if(s==NULL||item<=FACTORY_ITEM_NONE
    ||item>FACTORY_ITEM_CONSTRUCTION_MATERIAL||quantity==0U)
    return FACTORY_RESULT_INVALID_ARGUMENT;
 w=factory_cargo_wagon_store_find_mutable(&s->cargo_wagons,id);
 if(w==NULL)return factory_entity_is_valid(s->entities,id)
    ?FACTORY_RESULT_UNSUPPORTED_ENTITY:FACTORY_RESULT_ENTITY_NOT_FOUND;
 if((w->cargo_item!=FACTORY_ITEM_NONE&&w->cargo_item!=item)
    ||quantity>FACTORY_CARGO_WAGON_CAPACITY-w->cargo_quantity)
    return FACTORY_RESULT_INVALID_STATE;
 w->cargo_item=item;w->cargo_quantity+=quantity;return FACTORY_RESULT_OK;}

FactoryResult factory_simulation_cargo_wagon_remove(FactorySimulation*s,
    FactoryEntityId id,FactoryItemType item,uint32_t quantity)
{FactoryCargoWagon*w;if(s==NULL||item<=FACTORY_ITEM_NONE
    ||item>FACTORY_ITEM_CONSTRUCTION_MATERIAL||quantity==0U)
    return FACTORY_RESULT_INVALID_ARGUMENT;
 w=factory_cargo_wagon_store_find_mutable(&s->cargo_wagons,id);
 if(w==NULL)return factory_entity_is_valid(s->entities,id)
    ?FACTORY_RESULT_UNSUPPORTED_ENTITY:FACTORY_RESULT_ENTITY_NOT_FOUND;
 if(w->cargo_item!=item||quantity>w->cargo_quantity)return FACTORY_RESULT_INVALID_STATE;
 w->cargo_quantity-=quantity;if(w->cargo_quantity==0U)w->cargo_item=FACTORY_ITEM_NONE;
 return FACTORY_RESULT_OK;}

static int plan_compare(const void*a,const void*b)
{FactoryEntityId x=((const FactoryLocomotivePlan*)a)->id;
 FactoryEntityId y=((const FactoryLocomotivePlan*)b)->id;return x<y?-1:x>y;}
static FactoryDirection opposite_direction(FactoryDirection d)
{return (FactoryDirection)(((uint32_t)d+2U)%4U);}

void factory_locomotives_update(FactorySimulation*s)
{
    FactoryLocomotiveStore*store=&s->locomotives;
    for(size_t i=0;i<store->count;++i)store->plans[i]=(FactoryLocomotivePlan){
        .id=store->items[i].entity_id,.from=store->items[i].rail_entity_id};
    if(store->count>1U)
        qsort(store->plans,store->count,sizeof(*store->plans),plan_compare);
    for(size_t i=0;i<store->count;++i){FactoryLocomotive*l=
        factory_locomotive_store_find_mutable(store,store->plans[i].id);
        if(l->progress<FACTORY_LOCOMOTIVE_MOVE_TICKS)++l->progress;
        l->activity=FACTORY_LOCOMOTIVE_MOVING;
        if(l->progress<FACTORY_LOCOMOTIVE_MOVE_TICKS)continue;
        FactoryRailTraversal t={0};bool known=factory_simulation_get_rail_traversal(
            s,l->rail_entity_id,l->entry_direction,&t);
        if(!known){l->activity=FACTORY_LOCOMOTIVE_DISCONNECTED;continue;}
        if(!t.allowed){l->activity=factory_rail_switch_store_find(&s->rail_switches,
            l->rail_entity_id)!=NULL?FACTORY_LOCOMOTIVE_BLOCKED_SWITCH:
            FACTORY_LOCOMOTIVE_BLOCKED_TRACK;continue;}
        if(factory_simulation_get_rail_vehicle_occupant(s,t.exit_entity_id)!=0U){
            l->activity=FACTORY_LOCOMOTIVE_BLOCKED_OCCUPIED;continue;}
        store->plans[i].eligible=true;store->plans[i].move=true;
        store->plans[i].to=t.exit_entity_id;
        store->plans[i].next_entry=opposite_direction(t.exit_direction);
        for(size_t j=0;j<i;++j)if(store->plans[j].move
            &&store->plans[j].to==store->plans[i].to){store->plans[i].move=false;
                l->activity=FACTORY_LOCOMOTIVE_BLOCKED_OCCUPIED;break;}
    }
    for(size_t i=0;i<store->count;++i)if(store->plans[i].move){FactoryLocomotive*l=
        factory_locomotive_store_find_mutable(store,store->plans[i].id);
        FactoryEntityId previous=l->rail_entity_id,carry_rail=l->rail_entity_id;
        FactoryDirection carry_entry=l->entry_direction;
        FactoryEntityId wagon_id=l->rear_vehicle_id;
        l->rail_entity_id=store->plans[i].to;
        l->entry_direction=store->plans[i].next_entry;l->progress=0U;
        l->activity=FACTORY_LOCOMOTIVE_MOVING;
        while(wagon_id!=0U){FactoryCargoWagon*w=
            factory_cargo_wagon_store_find_mutable(&s->cargo_wagons,wagon_id);
            FactoryEntityId next=w->next_vehicle_id,old_rail=w->rail_entity_id;
            FactoryDirection old_entry=w->entry_direction;
            w->rail_entity_id=carry_rail;w->entry_direction=carry_entry;
            carry_rail=old_rail;carry_entry=old_entry;wagon_id=next;}
        factory_simulation_emit_event(s,(FactoryEvent){.type=FACTORY_EVENT_LOCOMOTIVE_MOVED,
            .entity_id=l->entity_id,.related_entity_id=previous,
            .quantity=l->rail_entity_id,.related_quantity=l->vehicle_count});}
}
