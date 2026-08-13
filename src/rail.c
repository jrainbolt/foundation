#include "rail_internal.h"
#include "simulation_internal.h"
#include "tick_preflight_internal.h"

#include <stdlib.h>

static const int32_t direction_x[4]={0,1,0,-1};
static const int32_t direction_y[4]={-1,0,1,0};
static int plan_compare(const void*a,const void*b);

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
STORE_FUNCTIONS(factory_rail_signal_store,FactoryRailSignalStore,FactoryRailSignal)
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

void factory_rail_signal_store_add(FactoryRailSignalStore*s,
    FactoryEntityId id,int32_t x,int32_t y,FactoryDirection orientation)
{s->items[s->count++]=(FactoryRailSignal){id,x,y,orientation};}

const FactoryRailSignal *factory_rail_signal_store_find(
    const FactoryRailSignalStore*s,FactoryEntityId id)
{if(s!=NULL)for(size_t i=0U;i<s->count;++i)
 if(s->items[i].entity_id==id)return &s->items[i];return NULL;}

void factory_rail_topology_destroy(FactoryRailTopology*topology)
{
    if(topology==NULL)return;
    free(topology->rails);free(topology->switches);free(topology->stations);
    free(topology->networks);free(topology->blocks);free(topology->block_members);
    *topology=(FactoryRailTopology){0};
}

static int rail_compare(const void*a,const void*b)
{FactoryEntityId x=((const FactoryRailInspection*)a)->entity_id,y=((const FactoryRailInspection*)b)->entity_id;return x<y?-1:x>y;}
static int switch_compare(const void*a,const void*b)
{FactoryEntityId x=((const FactoryRailSwitchInspection*)a)->entity_id,y=((const FactoryRailSwitchInspection*)b)->entity_id;return x<y?-1:x>y;}
static int block_member_compare(const void*a,const void*b)
{const FactoryRailBlockMember*x=a,*y=b;
 if(x->block_id!=y->block_id)return x->block_id<y->block_id?-1:1;
 return x->rail_id<y->rail_id?-1:x->rail_id>y->rail_id;}
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

static bool signal_boundary_between(const FactorySimulation*s,
    const FactoryRailTopology*t,FactoryEntityId a,FactoryEntityId b)
{for(size_t i=0U;i<s->rail_signals.count;++i){const FactoryRailSignal*signal=
    &s->rail_signals.items[i];uint32_t d=(uint32_t)signal->orientation;
    int32_t ax=signal->x+direction_y[d],ay=signal->y-direction_x[d];
    for(size_t n=0U;n<node_count(t);++n){Node attached=node_at_index(
        (FactoryRailTopology*)t,n);if(attached.x!=ax||attached.y!=ay)continue;
        FactoryEntityId upstream=attached.neighbors[(d+2U)%4U];
        if(upstream!=0U&&((*attached.entity_id==a&&upstream==b)
            ||(*attached.entity_id==b&&upstream==a)))return true;
    }}return false;}

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
            FACTORY_TOPOLOGY_RAIL,nodes,sizeof(*next.networks)))==NULL)
        ||(nodes!=0U&&(next.blocks=factory_topology_calloc(simulation,
            FACTORY_TOPOLOGY_RAIL,nodes,sizeof(*next.blocks)))==NULL)
        ||(nodes!=0U&&(next.block_members=factory_topology_calloc(simulation,
            FACTORY_TOPOLOGY_RAIL,nodes,sizeof(*next.block_members)))==NULL)){
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
    next.block_member_count=nodes;
    for(size_t i=0U;i<nodes;++i){Node node=node_at_index(&next,i);bool station=false;
        for(size_t j=0U;j<simulation->rail_stations.count;++j){
            const FactoryRailStation*st=&simulation->rail_stations.items[j];
            if(st->x+direction_x[st->orientation]==node.x
                &&st->y+direction_y[st->orientation]==node.y){station=true;break;}
        }
        bool boundary=node.is_switch||station||*node.connection_count!=2U;
        next.block_members[i]=(FactoryRailBlockMember){*node.entity_id,
            boundary?*node.entity_id:*node.entity_id};
    }
    {bool changed;do{changed=false;for(size_t i=0U;i<nodes;++i){Node node=
        node_at_index(&next,i);FactoryRailBlockMember*member=NULL;
        for(size_t m=0U;m<nodes;++m)if(next.block_members[m].rail_id
                ==*node.entity_id){member=&next.block_members[m];break;}
        bool station=false;for(size_t j=0U;j<simulation->rail_stations.count;++j){
            const FactoryRailStation*st=&simulation->rail_stations.items[j];
            if(st->x+direction_x[st->orientation]==node.x
                &&st->y+direction_y[st->orientation]==node.y){station=true;break;}}
        if(node.is_switch||station||*node.connection_count!=2U)continue;
        for(size_t d=0U;d<4U;++d)if(node.neighbors[d]!=0U){Node neighbor;
            if(find_node_by_id(&next,node.neighbors[d],&neighbor)){
                bool neighbor_station=false;
                for(size_t j=0U;j<simulation->rail_stations.count;++j){
                    const FactoryRailStation*st=&simulation->rail_stations.items[j];
                    if(st->x+direction_x[st->orientation]==neighbor.x
                        &&st->y+direction_y[st->orientation]==neighbor.y){
                        neighbor_station=true;break;}}
                if(neighbor.is_switch||neighbor_station
                    ||*neighbor.connection_count!=2U)continue;
                if(signal_boundary_between(simulation,&next,*node.entity_id,
                        *neighbor.entity_id))continue;
                FactoryRailBlockMember*other=NULL;for(size_t m=0U;m<nodes;++m)
                    if(next.block_members[m].rail_id==*neighbor.entity_id){
                        other=&next.block_members[m];break;}
                FactoryRailBlockId minimum=member->block_id<other->block_id
                    ?member->block_id:other->block_id;
                if(member->block_id!=minimum||other->block_id!=minimum){
                    member->block_id=minimum;other->block_id=minimum;changed=true;}
            }
        }
    }}while(changed);}
    if(nodes>1U)qsort(next.block_members,nodes,sizeof(*next.block_members),
        block_member_compare);
    for(size_t i=0U;i<nodes;){size_t end=i+1U;
        while(end<nodes&&next.block_members[end].block_id
            ==next.block_members[i].block_id)++end;
        FactoryRailBlock*block=&next.blocks[next.block_count++];
        *block=(FactoryRailBlock){.block_id=next.block_members[i].block_id,
            .member_offset=i,.member_count=end-i};
        if(end-i==1U){Node node;if(find_node_by_id(&next,
                next.block_members[i].rail_id,&node)){
            block->switch_boundary=node.is_switch;
            block->endpoint_boundary=*node.connection_count!=2U;
            for(size_t j=0U;j<simulation->rail_stations.count;++j){
                const FactoryRailStation*st=&simulation->rail_stations.items[j];
                if(st->x+direction_x[st->orientation]==node.x
                    &&st->y+direction_y[st->orientation]==node.y)
                    block->station_boundary=true;}
        }}i=end;
    }
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

FactoryRailBlockId factory_simulation_get_rail_block_for_rail(
    const FactorySimulation*s,FactoryEntityId rail_id)
{if(s!=NULL){for(size_t i=0U;i<s->rail_topology.block_member_count;++i){
    if(s->rail_topology.block_members[i].rail_id==rail_id)
        return s->rail_topology.block_members[i].block_id;}}
 return FACTORY_RAIL_BLOCK_NONE;}

size_t factory_simulation_get_rail_block_count(const FactorySimulation*s)
{return s!=NULL?s->rail_topology.block_count:0U;}

static FactoryTrainId reserved_block_owner(const FactorySimulation*s,
    FactoryRailBlockId block)
{for(size_t i=0U;i<s->locomotives.count;++i){
    if(s->locomotives.items[i].reserved_block_id==block)
        return s->locomotives.items[i].entity_id;}
 return FACTORY_TRAIN_NONE;}

static bool train_occupies_block(const FactorySimulation*s,FactoryTrainId train,
    FactoryRailBlockId block)
{const FactoryLocomotive*l=factory_locomotive_store_find(&s->locomotives,train);
 if(l!=NULL&&factory_simulation_get_rail_block_for_rail(s,l->rail_entity_id)==block)
    return true;
 for(size_t i=0U;i<s->cargo_wagons.count;++i){const FactoryCargoWagon*w=
    &s->cargo_wagons.items[i];if(w->train_id==train
        &&factory_simulation_get_rail_block_for_rail(s,w->rail_entity_id)==block)
        return true;}return false;}

static FactoryTrainId block_physical_blocker(const FactorySimulation*s,
    FactoryRailBlockId block,FactoryTrainId requester)
{for(size_t i=0U;i<s->locomotives.count;++i){const FactoryLocomotive*l=
    &s->locomotives.items[i];if(l->entity_id!=requester
        &&factory_simulation_get_rail_block_for_rail(s,l->rail_entity_id)==block)
        return l->entity_id;}
 for(size_t i=0U;i<s->cargo_wagons.count;++i){const FactoryCargoWagon*w=
    &s->cargo_wagons.items[i];FactoryTrainId owner=w->train_id!=0U?w->train_id:
        w->entity_id;if(owner!=requester
        &&factory_simulation_get_rail_block_for_rail(s,w->rail_entity_id)==block)
        return owner;}return FACTORY_TRAIN_NONE;}

bool factory_simulation_get_rail_block_at(const FactorySimulation*s,size_t index,
    FactoryRailBlockInspection*out)
{if(s==NULL||out==NULL||index>=s->rail_topology.block_count)return false;
 const FactoryRailBlock*b=&s->rail_topology.blocks[index];uint32_t occupied=0U;
 for(size_t i=0U;i<s->locomotives.count;++i)if(train_occupies_block(s,
    s->locomotives.items[i].entity_id,b->block_id))++occupied;
 *out=(FactoryRailBlockInspection){b->block_id,(uint32_t)b->member_count,
    reserved_block_owner(s,b->block_id),occupied,b->switch_boundary,
    b->station_boundary,b->endpoint_boundary};return true;}

bool factory_simulation_get_rail_block_member(const FactorySimulation*s,
    FactoryRailBlockId block,size_t index,FactoryEntityId*out)
{if(s==NULL||out==NULL)return false;for(size_t i=0U;
 i<s->rail_topology.block_count;++i){const FactoryRailBlock*b=
    &s->rail_topology.blocks[i];if(b->block_id==block&&index<b->member_count){
        *out=s->rail_topology.block_members[b->member_offset+index].rail_id;
        return true;}}return false;}

bool factory_simulation_get_rail_signal(const FactorySimulation*s,
    FactoryEntityId id,FactoryRailSignalInspection*out)
{if(s==NULL||out==NULL)return false;const FactoryRailSignal*signal=
    factory_rail_signal_store_find(&s->rail_signals,id);if(signal==NULL)return false;
 uint32_t d=(uint32_t)signal->orientation;Node attached;FactoryEntityId rail=0U,
    upstream=0U;FactoryRailBlockId down=0U,up=0U;bool connected=false;
 if(d<4U&&find_node_at((FactoryRailTopology*)&s->rail_topology,
        signal->x+direction_y[d],signal->y-direction_x[d],&attached)){
    rail=*attached.entity_id;upstream=attached.neighbors[(d+2U)%4U];
    connected=upstream!=0U&&(attached.port_mask&(UINT32_C(1)<<((d+2U)%4U)))!=0U;
    if(connected){down=factory_simulation_get_rail_block_for_rail(s,rail);
        up=factory_simulation_get_rail_block_for_rail(s,upstream);}}
 FactoryTrainId owner=connected?reserved_block_owner(s,down):0U;uint32_t occupied=0U;
 if(connected)for(size_t i=0U;i<s->locomotives.count;++i)
    if(train_occupies_block(s,s->locomotives.items[i].entity_id,down))++occupied;
 if(connected&&occupied==0U&&block_physical_blocker(s,down,0U)!=0U)occupied=1U;
 FactoryRailSignalAspect aspect=!connected||occupied!=0U?FACTORY_RAIL_SIGNAL_RED:
    owner!=0U?FACTORY_RAIL_SIGNAL_RESERVED:FACTORY_RAIL_SIGNAL_GREEN;
 *out=(FactoryRailSignalInspection){id,signal->x,signal->y,signal->orientation,
    rail,upstream,up,down,aspect,owner,occupied,connected};return true;}

void factory_locomotive_store_destroy(FactoryLocomotiveStore*s)
{if(s!=NULL){for(size_t i=0U;i<s->count;++i)free(s->items[i].route);
 free(s->items);free(s->plans);*s=(FactoryLocomotiveStore){0};}}

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
{if(s!=NULL)for(size_t i=0;i<s->count;++i)if(s->items[i].entity_id==id){free(s->items[i].route);--s->count;s->items[i]=s->items[s->count];return true;}return false;}

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
    if(l->route_status==FACTORY_TRAIN_ROUTE_ARRIVED)
        activity=FACTORY_LOCOMOTIVE_ARRIVED;
    else if(l->route_status==FACTORY_TRAIN_ROUTE_INVALID)
        activity=FACTORY_LOCOMOTIVE_ROUTE_INVALID;
    else if(l->progress==FACTORY_LOCOMOTIVE_MOVE_TICKS){
        if(!exists)activity=FACTORY_LOCOMOTIVE_DISCONNECTED;
        else if(!traversed)activity=factory_rail_switch_store_find(
            &s->rail_switches,l->rail_entity_id)!=NULL
                ?FACTORY_LOCOMOTIVE_BLOCKED_SWITCH:FACTORY_LOCOMOTIVE_BLOCKED_TRACK;
        else {FactoryEntityId occupant=factory_simulation_get_rail_vehicle_occupant(
            s,t.exit_entity_id);if(occupant!=0U&&occupant!=l->entity_id)
                activity=FACTORY_LOCOMOTIVE_BLOCKED_OCCUPIED;}
    }
    FactoryEntityId next=traversed?t.exit_entity_id:0U;
    FactoryDirection travel=traversed?t.exit_direction:l->entry_direction;
    if(l->route_status==FACTORY_TRAIN_ROUTE_ACTIVE
        &&l->route_index+1U<l->route_length){
        next=l->route[l->route_index+1U].rail_entity_id;
        travel=(FactoryDirection)(((uint32_t)
            l->route[l->route_index+1U].entry_direction+2U)%4U);
    }
    *out=(FactoryLocomotiveInspection){l->entity_id,l->rail_entity_id,x,y,
        l->entry_direction,travel,
        l->progress,FACTORY_LOCOMOTIVE_MOVE_TICKS,next,
        network,activity,l->entity_id,l->vehicle_count,l->destination_station_id,
        l->route_status,(uint32_t)l->route_length,(uint32_t)l->route_index,
        l->route_status==FACTORY_TRAIN_ROUTE_ACTIVE
            &&l->route_index+1U<l->route_length
            ?l->route[l->route_index+1U].rail_entity_id:0U,
        factory_simulation_get_rail_block_for_rail(s,l->rail_entity_id),
        l->route_status==FACTORY_TRAIN_ROUTE_ACTIVE
            &&l->route_index+1U<l->route_length
            ?factory_simulation_get_rail_block_for_rail(s,
                l->route[l->route_index+1U].rail_entity_id):0U,
        l->reserved_block_id,l->reservation_status,l->blocking_train_id};
    return true;
}

bool factory_simulation_get_train_route_step(const FactorySimulation*s,
    FactoryTrainId train_id,size_t index,FactoryTrainRouteStep*out)
{
    const FactoryLocomotive*l;
    if(s==NULL||out==NULL)return false;
    l=factory_locomotive_store_find(&s->locomotives,train_id);
    if(l==NULL||index>=l->route_length)return false;
    *out=l->route[index];return true;
}

typedef struct {FactoryTrainRouteStep state;size_t predecessor;} RouteSearchNode;

static bool route_state_equal(FactoryTrainRouteStep a,FactoryTrainRouteStep b)
{return a.rail_entity_id==b.rail_entity_id&&a.entry_direction==b.entry_direction;}

static size_t physical_route_neighbors(const FactorySimulation*s,
    FactoryTrainRouteStep state,FactoryTrainRouteStep out[2])
{
    FactoryRailInspection r;FactoryRailSwitchInspection w;size_t count=0U;
    if(factory_simulation_get_rail(s,state.rail_entity_id,&r)){
        if((r.port_mask&(UINT32_C(1)<<(uint32_t)state.entry_direction))==0U
            ||r.neighbors[state.entry_direction]==0U)return 0U;
        for(uint32_t d=0U;d<4U;++d)if(d!=(uint32_t)state.entry_direction
            &&(r.port_mask&(UINT32_C(1)<<d))!=0U&&r.neighbors[d]!=0U)
            out[count++]=(FactoryTrainRouteStep){r.neighbors[d],
                (FactoryDirection)((d+2U)%4U)};
    }else if(factory_simulation_get_rail_switch(s,state.rail_entity_id,&w)){
        if(w.neighbors[state.entry_direction]==0U)return 0U;
        if(state.entry_direction==w.stem_direction){
            FactoryDirection exits[2]={w.branch_a_direction,w.branch_b_direction};
            for(size_t i=0U;i<2U;++i)if(w.neighbors[exits[i]]!=0U)
                out[count++]=(FactoryTrainRouteStep){w.neighbors[exits[i]],
                    (FactoryDirection)(((uint32_t)exits[i]+2U)%4U)};
        }else if(state.entry_direction==w.branch_a_direction
            ||state.entry_direction==w.branch_b_direction){
            if(w.neighbors[w.stem_direction]!=0U)
                out[count++]=(FactoryTrainRouteStep){w.neighbors[w.stem_direction],
                    (FactoryDirection)(((uint32_t)w.stem_direction+2U)%4U)};
        }
    }
    if(count==2U&&out[1].rail_entity_id<out[0].rail_entity_id){
        FactoryTrainRouteStep temporary=out[0];out[0]=out[1];out[1]=temporary;
    }
    return count;
}

static FactoryResult plan_route(const FactorySimulation*s,
    const FactoryLocomotive*l,FactoryEntityId target,
    FactoryTrainRouteStep**out_route,size_t*out_length)
{
    size_t rail_nodes=s->rail_topology.rail_count+s->rail_topology.switch_count;
    if(rail_nodes==0U||rail_nodes>SIZE_MAX/4U)
        return FACTORY_RESULT_INVALID_STATE;
    size_t capacity=rail_nodes*4U;
    if(capacity>SIZE_MAX/sizeof(RouteSearchNode))return FACTORY_RESULT_OUT_OF_MEMORY;
    RouteSearchNode*nodes=malloc(capacity*sizeof(*nodes));
    if(nodes==NULL)return FACTORY_RESULT_OUT_OF_MEMORY;
    nodes[0]=(RouteSearchNode){{l->rail_entity_id,l->entry_direction},SIZE_MAX};
    size_t count=1U,head=0U,found=SIZE_MAX;
    while(head<count){RouteSearchNode current=nodes[head];
        if(current.state.rail_entity_id==target){found=head;break;}
        FactoryTrainRouteStep adjacent[2];size_t n=physical_route_neighbors(s,
            current.state,adjacent);
        for(size_t j=0U;j<n;++j){bool seen=false;
            for(size_t k=0U;k<count;++k)if(route_state_equal(nodes[k].state,
                    adjacent[j])){seen=true;break;}
            if(!seen){if(count==capacity){free(nodes);return FACTORY_RESULT_INVALID_STATE;}
                nodes[count]=(RouteSearchNode){adjacent[j],head};++count;}
        }
        ++head;
    }
    if(found==SIZE_MAX){free(nodes);return FACTORY_RESULT_INVALID_STATE;}
    size_t length=1U;for(size_t p=found;nodes[p].predecessor!=SIZE_MAX;
        p=nodes[p].predecessor)++length;
    if(length>FACTORY_TRAIN_ROUTE_MAX_STEPS){free(nodes);
        return FACTORY_RESULT_INVALID_STATE;}
    FactoryTrainRouteStep*route=malloc(length*sizeof(*route));
    if(route==NULL){free(nodes);return FACTORY_RESULT_OUT_OF_MEMORY;}
    size_t p=found;for(size_t i=length;i>0U;--i){route[i-1U]=nodes[p].state;
        p=nodes[p].predecessor;}
    free(nodes);*out_route=route;*out_length=length;return FACTORY_RESULT_OK;
}

FactoryResult factory_train_set_destination(FactorySimulation*s,
    FactoryTrainId train_id,FactoryEntityId station_id,bool replan)
{
    FactoryLocomotive*l=factory_locomotive_store_find_mutable(&s->locomotives,
        train_id);FactoryRailStationInspection station;
    if(l==NULL)return factory_entity_is_valid(s->entities,train_id)
        ?FACTORY_RESULT_UNSUPPORTED_ENTITY:FACTORY_RESULT_ENTITY_NOT_FOUND;
    if(replan)station_id=l->destination_station_id;
    if(station_id==0U||!factory_simulation_get_rail_station(s,station_id,&station))
        return station_id!=0U&&factory_entity_is_valid(s->entities,station_id)
            ?FACTORY_RESULT_UNSUPPORTED_ENTITY:FACTORY_RESULT_ENTITY_NOT_FOUND;
    if(!station.connected)return FACTORY_RESULT_INVALID_STATE;
    FactoryTrainRouteStep*route=NULL;size_t length=0U;
    FactoryResult result=plan_route(s,l,station.attached_rail_id,&route,&length);
    if(result!=FACTORY_RESULT_OK)return result;
    FactoryEntityId old=l->destination_station_id;free(l->route);l->route=route;
    l->route_length=length;l->route_index=0U;l->destination_station_id=station_id;
    l->route_status=length==1U?FACTORY_TRAIN_ROUTE_ARRIVED:
        FACTORY_TRAIN_ROUTE_ACTIVE;
    FactoryRailBlockId required=length>1U
        ?factory_simulation_get_rail_block_for_rail(s,route[1].rail_entity_id):0U;
    if(l->reserved_block_id!=0U&&l->reserved_block_id!=required){
        factory_simulation_emit_event(s,(FactoryEvent){
            .type=FACTORY_EVENT_TRAIN_BLOCK_RELEASED,.entity_id=train_id,
            .quantity=l->reserved_block_id});l->reserved_block_id=0U;}
    if(old!=station_id)factory_simulation_emit_event(s,(FactoryEvent){
        .type=FACTORY_EVENT_TRAIN_DESTINATION_CHANGED,.entity_id=train_id,
        .related_entity_id=station_id,.quantity=old});
    if(length==1U)factory_simulation_emit_event(s,(FactoryEvent){
        .type=FACTORY_EVENT_TRAIN_ARRIVED,.entity_id=train_id,
        .related_entity_id=station_id,.quantity=l->rail_entity_id});
    return FACTORY_RESULT_OK;
}

FactoryResult factory_train_clear_destination(FactorySimulation*s,
    FactoryTrainId train_id)
{
    FactoryLocomotive*l=factory_locomotive_store_find_mutable(&s->locomotives,
        train_id);if(l==NULL)return factory_entity_is_valid(s->entities,train_id)
        ?FACTORY_RESULT_UNSUPPORTED_ENTITY:FACTORY_RESULT_ENTITY_NOT_FOUND;
    FactoryEntityId old=l->destination_station_id;if(l->reserved_block_id!=0U){
        factory_simulation_emit_event(s,(FactoryEvent){
            .type=FACTORY_EVENT_TRAIN_BLOCK_RELEASED,.entity_id=train_id,
            .quantity=l->reserved_block_id});l->reserved_block_id=0U;}
    free(l->route);l->route=NULL;
    l->route_length=0U;l->route_index=0U;l->destination_station_id=0U;
    l->route_status=FACTORY_TRAIN_ROUTE_NONE;
    if(old!=0U)factory_simulation_emit_event(s,(FactoryEvent){
        .type=FACTORY_EVENT_TRAIN_DESTINATION_CHANGED,.entity_id=train_id,
        .quantity=old});
    return FACTORY_RESULT_OK;
}

bool factory_train_routes_validate(const FactorySimulation*s)
{
    if(s==NULL)return false;
    for(size_t i=0U;i<s->locomotives.count;++i){const FactoryLocomotive*l=
        &s->locomotives.items[i];
        if(l->route_status==FACTORY_TRAIN_ROUTE_NONE){
            if(l->destination_station_id!=0U||l->route_length!=0U
                ||l->route_index!=0U||l->route!=NULL)return false;
            continue;
        }
        if(l->route_length==0U||l->route==NULL||l->route_index>=l->route_length)
            return false;
        if(l->route_status==FACTORY_TRAIN_ROUTE_INVALID)continue;
        FactoryRailStationInspection station;
        if(!factory_simulation_get_rail_station(s,l->destination_station_id,
                &station)||!station.connected
            ||l->route[l->route_length-1U].rail_entity_id
                !=station.attached_rail_id
            ||l->route[l->route_index].rail_entity_id!=l->rail_entity_id
            ||l->route[l->route_index].entry_direction!=l->entry_direction
            ||(l->route_status==FACTORY_TRAIN_ROUTE_ACTIVE
                &&l->route_index+1U>=l->route_length)
            ||(l->route_status==FACTORY_TRAIN_ROUTE_ARRIVED
                &&l->route_index+1U!=l->route_length))return false;
        for(size_t step=0U;step+1U<l->route_length;++step){
            FactoryTrainRouteStep adjacent[2];size_t n=physical_route_neighbors(s,
                l->route[step],adjacent);bool found=false;
            for(size_t j=0U;j<n;++j)if(route_state_equal(adjacent[j],
                    l->route[step+1U]))found=true;
            if(!found)return false;
        }
    }
    return true;
}

void factory_train_reservations_update(FactorySimulation*s)
{
    FactoryLocomotiveStore*store=&s->locomotives;
    for(size_t i=0U;i<store->count;++i)store->plans[i]=(FactoryLocomotivePlan){
        .id=store->items[i].entity_id};
    if(store->count>1U)qsort(store->plans,store->count,sizeof(*store->plans),
        plan_compare);
    for(size_t i=0U;i<store->count;++i){FactoryLocomotive*l=
        factory_locomotive_store_find_mutable(store,store->plans[i].id);
        l->blocking_train_id=0U;l->reservation_status=
            l->reserved_block_id!=0U?FACTORY_TRAIN_RESERVATION_HELD:
            FACTORY_TRAIN_RESERVATION_NONE;
        FactoryRailBlockId required=0U;
        if(l->route_status==FACTORY_TRAIN_ROUTE_ACTIVE
            &&l->route_index+1U<l->route_length)
            required=factory_simulation_get_rail_block_for_rail(s,
                l->route[l->route_index+1U].rail_entity_id);
        if(required!=0U&&train_occupies_block(s,l->entity_id,required))required=0U;
        if(l->reserved_block_id!=0U&&l->reserved_block_id!=required){
            factory_simulation_emit_event(s,(FactoryEvent){
                .type=FACTORY_EVENT_TRAIN_BLOCK_RELEASED,.entity_id=l->entity_id,
                .quantity=l->reserved_block_id});l->reserved_block_id=0U;
            l->reservation_status=FACTORY_TRAIN_RESERVATION_NONE;
        }
        if(required==0U||l->reserved_block_id==required)continue;
        FactoryTrainId blocker=reserved_block_owner(s,required);
        if(blocker==0U)blocker=block_physical_blocker(s,required,l->entity_id);
        if(blocker!=0U){l->reservation_status=FACTORY_TRAIN_RESERVATION_WAITING;
            l->blocking_train_id=blocker;continue;}
        l->reserved_block_id=required;
        l->reservation_status=FACTORY_TRAIN_RESERVATION_HELD;
        factory_simulation_emit_event(s,(FactoryEvent){
            .type=FACTORY_EVENT_TRAIN_BLOCK_RESERVED,.entity_id=l->entity_id,
            .quantity=required});
    }
}

void factory_train_reservations_release_for_topology(FactorySimulation*s)
{FactoryLocomotiveStore*store=&s->locomotives;
 for(size_t i=0U;i<store->count;++i)store->plans[i]=(FactoryLocomotivePlan){
    .id=store->items[i].entity_id};
 if(store->count>1U)qsort(store->plans,store->count,sizeof(*store->plans),
    plan_compare);
 for(size_t i=0U;i<store->count;++i){FactoryLocomotive*l=
    factory_locomotive_store_find_mutable(store,store->plans[i].id);
    if(l->reserved_block_id!=0U){
        factory_simulation_emit_event(s,(FactoryEvent){
            .type=FACTORY_EVENT_TRAIN_BLOCK_RELEASED,.entity_id=l->entity_id,
            .quantity=l->reserved_block_id});l->reserved_block_id=0U;
        l->reservation_status=FACTORY_TRAIN_RESERVATION_NONE;
        l->blocking_train_id=0U;}}}

bool factory_train_reservations_validate(const FactorySimulation*s)
{for(size_t i=0U;i<s->locomotives.count;++i){const FactoryLocomotive*l=
    &s->locomotives.items[i];if(l->reserved_block_id==0U)continue;
    if(l->route_status!=FACTORY_TRAIN_ROUTE_ACTIVE
        ||factory_simulation_get_rail_block_for_rail(s,l->reserved_block_id)==0U)
        return false;
    bool exists=false;for(size_t b=0U;b<s->rail_topology.block_count;++b)
        if(s->rail_topology.blocks[b].block_id==l->reserved_block_id)exists=true;
    if(!exists)return false;
    FactoryRailBlockId required=factory_simulation_get_rail_block_for_rail(s,
        l->route[l->route_index+1U].rail_entity_id);
    if(required!=l->reserved_block_id)return false;
    for(size_t j=i+1U;j<s->locomotives.count;++j)
        if(s->locomotives.items[j].reserved_block_id==l->reserved_block_id)
            return false;
 }return true;}

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
        if(l->route_status==FACTORY_TRAIN_ROUTE_ARRIVED){
            l->progress=FACTORY_LOCOMOTIVE_MOVE_TICKS;
            l->activity=FACTORY_LOCOMOTIVE_ARRIVED;continue;
        }
        if(l->route_status==FACTORY_TRAIN_ROUTE_INVALID){
            l->progress=FACTORY_LOCOMOTIVE_MOVE_TICKS;
            l->activity=FACTORY_LOCOMOTIVE_ROUTE_INVALID;continue;
        }
        if(l->progress<FACTORY_LOCOMOTIVE_MOVE_TICKS)continue;
        FactoryRailTraversal t={0};bool known=factory_simulation_get_rail_traversal(
            s,l->rail_entity_id,l->entry_direction,&t);
        if(l->route_status==FACTORY_TRAIN_ROUTE_ACTIVE){
            FactoryRailStationInspection station;FactoryTrainRouteStep adjacent[2];
            FactoryTrainRouteStep current={l->rail_entity_id,l->entry_direction};
            bool current_matches=l->route_index<l->route_length
                &&route_state_equal(current,l->route[l->route_index]);
            size_t adjacent_count=physical_route_neighbors(s,current,adjacent);
            FactoryTrainRouteStep desired={0};bool transition=false;
            if(l->route_index+1U<l->route_length){
                desired=l->route[l->route_index+1U];
                for(size_t j=0U;j<adjacent_count;++j)
                    if(route_state_equal(adjacent[j],desired))transition=true;
            }
            if(!factory_simulation_get_rail_station(s,l->destination_station_id,
                    &station)||!station.connected||!current_matches||!transition){
                l->route_status=FACTORY_TRAIN_ROUTE_INVALID;
                l->activity=FACTORY_LOCOMOTIVE_ROUTE_INVALID;
                if(l->reserved_block_id!=0U){factory_simulation_emit_event(s,
                    (FactoryEvent){.type=FACTORY_EVENT_TRAIN_BLOCK_RELEASED,
                    .entity_id=l->entity_id,.quantity=l->reserved_block_id});
                    l->reserved_block_id=0U;}
                factory_simulation_emit_event(s,(FactoryEvent){
                    .type=FACTORY_EVENT_TRAIN_ROUTE_INVALIDATED,
                    .entity_id=l->entity_id,
                    .related_entity_id=l->destination_station_id,
                    .quantity=desired.rail_entity_id});continue;
            }
            if(!known||!t.allowed||t.exit_entity_id!=desired.rail_entity_id){
                l->activity=FACTORY_LOCOMOTIVE_BLOCKED_SWITCH;continue;
            }
        }
        if(!known){l->activity=FACTORY_LOCOMOTIVE_DISCONNECTED;continue;}
        if(!t.allowed){l->activity=factory_rail_switch_store_find(&s->rail_switches,
            l->rail_entity_id)!=NULL?FACTORY_LOCOMOTIVE_BLOCKED_SWITCH:
            FACTORY_LOCOMOTIVE_BLOCKED_TRACK;continue;}
        FactoryRailBlockId from_block=factory_simulation_get_rail_block_for_rail(
            s,l->rail_entity_id);FactoryRailBlockId to_block=
            factory_simulation_get_rail_block_for_rail(s,t.exit_entity_id);
        if(to_block==0U){l->activity=FACTORY_LOCOMOTIVE_DISCONNECTED;continue;}
        if(to_block!=from_block&&l->reserved_block_id!=to_block){
            l->activity=FACTORY_LOCOMOTIVE_BLOCKED_RESERVATION;continue;}
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
        if(l->reserved_block_id!=0U&&l->reserved_block_id==
            factory_simulation_get_rail_block_for_rail(s,l->rail_entity_id)){
            factory_simulation_emit_event(s,(FactoryEvent){
                .type=FACTORY_EVENT_TRAIN_BLOCK_RELEASED,.entity_id=l->entity_id,
                .quantity=l->reserved_block_id});l->reserved_block_id=0U;
            l->reservation_status=FACTORY_TRAIN_RESERVATION_NONE;
        }
        if(l->route_status==FACTORY_TRAIN_ROUTE_ACTIVE){
            ++l->route_index;
            if(l->route_index+1U==l->route_length){
                l->route_status=FACTORY_TRAIN_ROUTE_ARRIVED;
                l->activity=FACTORY_LOCOMOTIVE_ARRIVED;
            }
        }
        while(wagon_id!=0U){FactoryCargoWagon*w=
            factory_cargo_wagon_store_find_mutable(&s->cargo_wagons,wagon_id);
            FactoryEntityId next=w->next_vehicle_id,old_rail=w->rail_entity_id;
            FactoryDirection old_entry=w->entry_direction;
            w->rail_entity_id=carry_rail;w->entry_direction=carry_entry;
            carry_rail=old_rail;carry_entry=old_entry;wagon_id=next;}
        factory_simulation_emit_event(s,(FactoryEvent){.type=FACTORY_EVENT_LOCOMOTIVE_MOVED,
            .entity_id=l->entity_id,.related_entity_id=previous,
            .quantity=l->rail_entity_id,.related_quantity=l->vehicle_count});
        if(l->route_status==FACTORY_TRAIN_ROUTE_ARRIVED)
            factory_simulation_emit_event(s,(FactoryEvent){
                .type=FACTORY_EVENT_TRAIN_ARRIVED,.entity_id=l->entity_id,
                .related_entity_id=l->destination_station_id,
                .quantity=l->rail_entity_id});
    }
}
