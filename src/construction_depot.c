#include "construction_depot_internal.h"

#include <stdlib.h>

void factory_construction_depot_store_destroy(FactoryConstructionDepotStore *s)
{
    if(s==NULL)return;
    free(s->items);
    *s=(FactoryConstructionDepotStore){0};
}
bool factory_construction_depot_store_reserve_one(FactoryConstructionDepotStore *s)
{
    FactoryConstructionDepot *p;
    size_t capacity;
    if(s==NULL)return false;
    if(s->count<s->capacity)return true;
    capacity=s->capacity==0U?4U:s->capacity*2U;
    if(capacity<s->capacity||capacity>SIZE_MAX/sizeof(*p))return false;
    p=realloc(s->items,capacity*sizeof(*p));
    if(p==NULL)return false;
    s->items=p;
    s->capacity=capacity;
    return true;
}
void factory_construction_depot_store_add(FactoryConstructionDepotStore *s,
    FactoryEntityId id,int32_t x,int32_t y)
{s->items[s->count++]=(FactoryConstructionDepot){id,x,y,0U};}
const FactoryConstructionDepot *factory_construction_depot_store_find(
    const FactoryConstructionDepotStore *s,FactoryEntityId id)
{
    if(s==NULL||id==0U)return NULL;
    for(size_t i=0U;i<s->count;++i)
        if(s->items[i].entity_id==id)return &s->items[i];
    return NULL;
}
FactoryConstructionDepot *factory_construction_depot_store_find_mutable(
    FactoryConstructionDepotStore *s,FactoryEntityId id)
{return (FactoryConstructionDepot *)factory_construction_depot_store_find(s,id);}
bool factory_construction_depot_store_remove(FactoryConstructionDepotStore *s,
    FactoryEntityId id)
{
    if(s==NULL)return false;
    for(size_t i=0U;i<s->count;++i){
        if(s->items[i].entity_id!=id)continue;
        --s->count;
        s->items[i]=s->items[s->count];
        return true;
    }
    return false;
}
