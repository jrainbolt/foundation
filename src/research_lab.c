#include "research_lab_internal.h"

#include "foundation/content.h"
#include "event_internal.h"
#include "research_internal.h"
#include "simulation_internal.h"

#include <stdlib.h>

void factory_research_lab_store_destroy(FactoryResearchLabStore *s)
{if(s!=NULL){free(s->items);*s=(FactoryResearchLabStore){0};}}
bool factory_research_lab_store_reserve_one(FactoryResearchLabStore*s)
{if(s==NULL)return false;if(s->count<s->capacity)return true;size_t c=s->capacity==0U?4U:s->capacity*2U;if(c<s->capacity||c>SIZE_MAX/sizeof(*s->items))return false;void*p=realloc(s->items,c*sizeof(*s->items));if(p==NULL)return false;s->items=p;s->capacity=c;return true;}
void factory_research_lab_store_add(FactoryResearchLabStore*s,FactoryEntityId id,int32_t x,int32_t y)
{s->items[s->count++]=(FactoryResearchLab){id,x,y,0U,FACTORY_RESEARCH_LAB_IDLE,0U,0U};}
const FactoryResearchLab *factory_research_lab_store_find(const FactoryResearchLabStore*s,FactoryEntityId id)
{if(s==NULL)return NULL;for(size_t i=0U;i<s->count;++i)if(s->items[i].entity_id==id)return &s->items[i];return NULL;}
FactoryResearchLab *factory_research_lab_store_find_mutable(FactoryResearchLabStore*s,FactoryEntityId id)
{return(FactoryResearchLab*)factory_research_lab_store_find(s,id);}
bool factory_research_lab_store_remove(FactoryResearchLabStore*s,FactoryEntityId id)
{if(s==NULL)return false;for(size_t i=0U;i<s->count;++i)if(s->items[i].entity_id==id){for(size_t j=i+1U;j<s->count;++j)s->items[j-1U]=s->items[j];--s->count;return true;}return false;}

FactoryResult factory_simulation_get_research_lab(const FactorySimulation*s,
    FactoryEntityId id,FactoryResearchLabInspection*out)
{
    const FactoryResearchLab*l;FactoryPowerConsumerInspection p={0};
    if(s==NULL||out==NULL)return FACTORY_RESULT_INVALID_ARGUMENT;
    l=factory_research_lab_store_find(&s->research_labs,id);
    if(l==NULL)return FACTORY_RESULT_ENTITY_NOT_FOUND;
    (void)factory_simulation_get_power_consumer(s,id,&p);
    *out=(FactoryResearchLabInspection){id,l->x,l->y,l->science_quantity,
        FACTORY_RESEARCH_LAB_SCIENCE_CAPACITY,p.network_id,p.connected,p.powered,
        l->activity,l->science_consumed_last_tick,l->work_contributed_last_tick};
    return FACTORY_RESULT_OK;
}

void factory_research_labs_update(FactorySimulation*s)
{
    const FactoryTechnologyDefinition*d=factory_technology_definition_get(s->research.active);
    FactoryTechnologyProgress*p=NULL;FactoryResearchLab*chosen=NULL;
    size_t count=s->research_labs.count;
    for(size_t i=0U;i<count;++i){FactoryResearchLab*l=&s->research_labs.items[i];l->science_consumed_last_tick=0U;l->work_contributed_last_tick=0U;l->activity=FACTORY_RESEARCH_LAB_IDLE;}
    if(d!=NULL)for(size_t i=0U;i<factory_technology_definition_count();++i)if(factory_content_technology_at(i)->id==d->id)p=&s->research.progress[i];
    for(size_t i=0U;i<count;++i){FactoryResearchLab*l=&s->research_labs.items[i];bool powered=factory_power_is_entity_powered(s,l->entity_id);if(!powered){l->activity=FACTORY_RESEARCH_LAB_UNPOWERED;continue;}if(d==NULL||p==NULL){l->activity=FACTORY_RESEARCH_LAB_NO_ACTIVE_RESEARCH;continue;}if(!p->science_committed&&l->science_quantity<d->science_quantity_per_unit){l->activity=FACTORY_RESEARCH_LAB_NO_SCIENCE;continue;}if(chosen==NULL||l->entity_id<chosen->entity_id)chosen=l;}
    if(chosen==NULL)return;
    for(size_t i=0U;i<count;++i){FactoryResearchLab*l=&s->research_labs.items[i];if(l==chosen)l->activity=FACTORY_RESEARCH_LAB_WORKING;else if(l->activity==FACTORY_RESEARCH_LAB_IDLE)l->activity=FACTORY_RESEARCH_LAB_WAITING;}
    if(!p->science_committed){chosen->science_quantity-=d->science_quantity_per_unit;chosen->science_consumed_last_tick=d->science_quantity_per_unit;p->science_committed=true;}
    ++p->work_ticks;chosen->work_contributed_last_tick=1U;
    if(p->work_ticks<d->work_ticks_per_unit)return;
    p->work_ticks=0U;p->science_committed=false;++p->completed_units;
    factory_simulation_emit_event(s,(FactoryEvent){.type=FACTORY_EVENT_RESEARCH_UNIT_COMPLETED,.entity_id=chosen->entity_id,.technology_id=d->id,.item_type=d->science_item,.quantity=d->science_quantity_per_unit,.related_quantity=p->completed_units,.third_quantity=d->required_science_units});
    if(p->completed_units==d->required_science_units){s->research.completed_bits|=UINT64_C(1)<<d->id;s->research.active=FACTORY_TECHNOLOGY_NONE;factory_simulation_emit_event(s,(FactoryEvent){.type=FACTORY_EVENT_TECHNOLOGY_COMPLETED,.technology_id=d->id,.quantity=p->completed_units});}
}
