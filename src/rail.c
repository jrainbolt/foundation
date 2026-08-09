#include "rail_internal.h"
#include "simulation_internal.h"
#include "tick_preflight_internal.h"

#include <stdlib.h>

uint32_t factory_rail_geometry_port_mask(FactoryRailGeometry g)
{
    static const uint32_t masks[FACTORY_RAIL_GEOMETRY_COUNT]={
        FACTORY_RAIL_PORT_EAST|FACTORY_RAIL_PORT_WEST,
        FACTORY_RAIL_PORT_NORTH|FACTORY_RAIL_PORT_SOUTH,
        FACTORY_RAIL_PORT_NORTH|FACTORY_RAIL_PORT_EAST,
        FACTORY_RAIL_PORT_NORTH|FACTORY_RAIL_PORT_WEST,
        FACTORY_RAIL_PORT_SOUTH|FACTORY_RAIL_PORT_EAST,
        FACTORY_RAIL_PORT_SOUTH|FACTORY_RAIL_PORT_WEST};
    return g>=0&&g<FACTORY_RAIL_GEOMETRY_COUNT?masks[g]:0U;
}
bool factory_rail_geometry_is_valid(FactoryRailGeometry g)
{return g>=0&&g<FACTORY_RAIL_GEOMETRY_COUNT;}

#define STORE_FUNCTIONS(prefix,Store,Item) \
void prefix##_destroy(Store*s){if(s!=NULL){free(s->items);*s=(Store){0};}} \
bool prefix##_reserve_one(Store*s){Item*p;size_t c;if(s==NULL)return false;if(s->count<s->capacity)return true;c=s->capacity==0U?4U:s->capacity*2U;if(c<s->capacity||c>SIZE_MAX/sizeof(*p))return false;p=realloc(s->items,c*sizeof(*p));if(p==NULL)return false;s->items=p;s->capacity=c;return true;} \
bool prefix##_remove(Store*s,FactoryEntityId id){if(s==NULL)return false;for(size_t i=0U;i<s->count;++i)if(s->items[i].entity_id==id){--s->count;s->items[i]=s->items[s->count];return true;}return false;}
STORE_FUNCTIONS(factory_rail_store,FactoryRailStore,FactoryRail)
STORE_FUNCTIONS(factory_rail_station_store,FactoryRailStationStore,FactoryRailStation)
#undef STORE_FUNCTIONS

void factory_rail_store_add(FactoryRailStore*s,FactoryEntityId id,int32_t x,
    int32_t y,FactoryRailGeometry g){s->items[s->count++]=(FactoryRail){id,x,y,g};}
const FactoryRail *factory_rail_store_find(const FactoryRailStore*s,
    FactoryEntityId id){if(s!=NULL)for(size_t i=0U;i<s->count;++i)if(s->items[i].entity_id==id)return &s->items[i];return NULL;}
void factory_rail_station_store_add(FactoryRailStationStore*s,
    FactoryEntityId id,int32_t x,int32_t y,FactoryDirection o)
{s->items[s->count++]=(FactoryRailStation){id,x,y,o};}
const FactoryRailStation *factory_rail_station_store_find(
    const FactoryRailStationStore*s,FactoryEntityId id)
{if(s!=NULL)for(size_t i=0U;i<s->count;++i)if(s->items[i].entity_id==id)return &s->items[i];return NULL;}
void factory_rail_topology_destroy(FactoryRailTopology*t)
{if(t!=NULL){free(t->rails);free(t->stations);free(t->networks);*t=(FactoryRailTopology){0};}}

static int rail_compare(const void*a,const void*b){FactoryEntityId x=((const FactoryRailInspection*)a)->entity_id,y=((const FactoryRailInspection*)b)->entity_id;return x<y?-1:x>y;}
static uint32_t opposite(uint32_t p){return p==FACTORY_RAIL_PORT_NORTH?FACTORY_RAIL_PORT_SOUTH:p==FACTORY_RAIL_PORT_EAST?FACTORY_RAIL_PORT_WEST:p==FACTORY_RAIL_PORT_SOUTH?FACTORY_RAIL_PORT_NORTH:p==FACTORY_RAIL_PORT_WEST?FACTORY_RAIL_PORT_EAST:0U;}
static FactoryRailInspection *rail_at(FactoryRailTopology*t,int32_t x,int32_t y)
{for(size_t i=0U;i<t->rail_count;++i)if(t->rails[i].x==x&&t->rails[i].y==y)return &t->rails[i];return NULL;}
static FactoryRailNetworkInspection *network_at(FactoryRailTopology*t,FactoryRailNetworkId id)
{for(size_t i=0U;i<t->network_count;++i)if(t->networks[i].network_id==id)return &t->networks[i];return NULL;}

FactoryResult factory_rail_topology_rebuild(FactorySimulation*s)
{
    FactoryRailTopology next={0};next.rail_count=s->rails.count;next.station_count=s->rail_stations.count;
    if((next.rail_count!=0U&&(next.rails=factory_topology_calloc(s,FACTORY_TOPOLOGY_RAIL,next.rail_count,sizeof(*next.rails)))==NULL)
        ||(next.station_count!=0U&&(next.stations=factory_topology_calloc(s,FACTORY_TOPOLOGY_RAIL,next.station_count,sizeof(*next.stations)))==NULL)
        ||(next.rail_count!=0U&&(next.networks=factory_topology_calloc(s,FACTORY_TOPOLOGY_RAIL,next.rail_count,sizeof(*next.networks)))==NULL)){factory_rail_topology_destroy(&next);return FACTORY_RESULT_OUT_OF_MEMORY;}
    for(size_t i=0U;i<next.rail_count;++i){const FactoryRail*r=&s->rails.items[i];next.rails[i]=(FactoryRailInspection){.entity_id=r->entity_id,.x=r->x,.y=r->y,.geometry=r->geometry,.port_mask=factory_rail_geometry_port_mask(r->geometry),.network_id=r->entity_id};}
    if(next.rail_count>1U)qsort(next.rails,next.rail_count,sizeof(*next.rails),rail_compare);
    for(size_t i=0U;i<next.rail_count;++i)for(size_t d=0U;d<4U;++d){static const int32_t dx[4]={0,1,0,-1},dy[4]={-1,0,1,0};uint32_t port=UINT32_C(1)<<d;FactoryRailInspection*n=rail_at(&next,next.rails[i].x+dx[d],next.rails[i].y+dy[d]);if(n!=NULL&&(next.rails[i].port_mask&port)!=0U&&(n->port_mask&opposite(port))!=0U){next.rails[i].neighbors[d]=n->entity_id;next.rails[i].connection_mask|=port;++next.rails[i].connection_count;}}
    {bool changed;do{changed=false;for(size_t i=0U;i<next.rail_count;++i)for(size_t d=0U;d<4U;++d)if(next.rails[i].neighbors[d]!=0U){for(size_t j=0U;j<next.rail_count;++j)if(next.rails[j].entity_id==next.rails[i].neighbors[d]){FactoryRailNetworkId m=next.rails[i].network_id<next.rails[j].network_id?next.rails[i].network_id:next.rails[j].network_id;if(next.rails[i].network_id!=m||next.rails[j].network_id!=m){next.rails[i].network_id=m;next.rails[j].network_id=m;changed=true;}break;}}}while(changed);}
    for(size_t i=0U;i<next.rail_count;++i){FactoryRailNetworkInspection*n=network_at(&next,next.rails[i].network_id);if(n==NULL){n=&next.networks[next.network_count++];n->network_id=next.rails[i].network_id;}++n->rail_count;}
    for(size_t i=0U;i<next.station_count;++i){const FactoryRailStation*st=&s->rail_stations.items[i];static const int32_t dx[4]={0,1,0,-1},dy[4]={-1,0,1,0};FactoryRailInspection*r=rail_at(&next,st->x+dx[st->orientation],st->y+dy[st->orientation]);next.stations[i]=(FactoryRailStationInspection){.entity_id=st->entity_id,.x=st->x,.y=st->y,.orientation=st->orientation,.attached_rail_id=r!=NULL?r->entity_id:0U,.network_id=r!=NULL?r->network_id:0U,.connected=r!=NULL};if(r!=NULL)++network_at(&next,r->network_id)->station_count;}
    factory_rail_topology_destroy(&s->rail_topology);s->rail_topology=next;return FACTORY_RESULT_OK;
}

bool factory_simulation_get_rail(const FactorySimulation*s,FactoryEntityId id,
    FactoryRailInspection*out){if(s==NULL||out==NULL)return false;for(size_t i=0U;i<s->rail_topology.rail_count;++i)if(s->rail_topology.rails[i].entity_id==id){*out=s->rail_topology.rails[i];return true;}return false;}
bool factory_simulation_get_rail_station(const FactorySimulation*s,
    FactoryEntityId id,FactoryRailStationInspection*out){if(s==NULL||out==NULL)return false;for(size_t i=0U;i<s->rail_topology.station_count;++i)if(s->rail_topology.stations[i].entity_id==id){*out=s->rail_topology.stations[i];return true;}return false;}
size_t factory_simulation_get_rail_network_count(const FactorySimulation*s)
{return s==NULL?0U:s->rail_topology.network_count;}
const FactoryRailNetworkInspection *factory_simulation_get_rail_network(
    const FactorySimulation*s,size_t i){return s!=NULL&&i<s->rail_topology.network_count?&s->rail_topology.networks[i]:NULL;}
