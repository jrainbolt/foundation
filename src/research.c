#include "research_internal.h"
#include "foundation/content.h"

#include "event_internal.h"
#include "simulation_internal.h"

#include <limits.h>

size_t factory_technology_definition_count(void)
{ return factory_content_technology_count(); }

const FactoryTechnologyDefinition *factory_technology_definition_at(size_t index)
{ return factory_content_technology_at(index); }

const FactoryTechnologyDefinition *factory_technology_definition_get(
    FactoryTechnologyId id)
{
    return factory_content_technology_get(id);
}

static bool visit_definition(const FactoryTechnologyDefinition *defs,
    size_t count,FactoryTechnologyId id,uint64_t *visiting,uint64_t *visited)
{
    const FactoryTechnologyDefinition *d=NULL;
    uint64_t bit=UINT64_C(1)<<id;
    if ((*visited&bit)!=0U) return true;
    if ((*visiting&bit)!=0U) return false;
    for (size_t i=0U;i<count;++i) if (defs[i].id==id) d=&defs[i];
    if (d==NULL) return false;
    *visiting|=bit;
    for (size_t i=0U;i<d->prerequisite_count;++i)
        if (!visit_definition(defs,count,d->prerequisites[i],visiting,visited))
            return false;
    *visiting&=~bit; *visited|=bit;
    return true;
}

bool factory_technology_definitions_validate(
    const FactoryTechnologyDefinition *defs,size_t count)
{
    uint64_t known=0U;
    if (defs==NULL || count==0U || count>63U) return false;
    for (size_t i=0U;i<count;++i) {
        const FactoryTechnologyDefinition *d=&defs[i];
        if (d->id==FACTORY_TECHNOLOGY_NONE || d->id>63U
            || d->prerequisite_count>FACTORY_TECHNOLOGY_MAX_PREREQUISITES
            || d->science_item<=FACTORY_ITEM_NONE
            || d->science_item>FACTORY_ITEM_BASIC_SCIENCE
            || d->science_quantity_per_unit==0U
            || d->required_science_units==0U || d->work_ticks_per_unit==0U
            || d->unlock_flags==0U || (d->unlock_flags&~FACTORY_UNLOCK_ALL)!=0U
            || (known&(UINT64_C(1)<<d->id))!=0U) return false;
        known|=UINT64_C(1)<<d->id;
        for (size_t p=0U;p<d->prerequisite_count;++p) {
            FactoryTechnologyId prerequisite=d->prerequisites[p];
            if (prerequisite==FACTORY_TECHNOLOGY_NONE || prerequisite==d->id)
                return false;
            for (size_t q=p+1U;q<d->prerequisite_count;++q)
                if (prerequisite==d->prerequisites[q]) return false;
        }
    }
    {
        uint64_t visiting=0U,visited=0U;
        for (size_t i=0U;i<count;++i)
            if (!visit_definition(defs,count,defs[i].id,&visiting,&visited))
                return false;
    }
    return true;
}

static size_t definition_index(FactoryTechnologyId id)
{
    for (size_t i=0U;i<factory_technology_definition_count();++i)
        if (factory_content_technology_at(i)->id==id) return i;
    return SIZE_MAX;
}

bool factory_simulation_is_technology_completed(
    const FactorySimulation *s,FactoryTechnologyId id)
{ return s!=NULL && id!=0U && id<64U
    && (s->research.completed_bits&(UINT64_C(1)<<id))!=0U; }

FactoryTechnologyId factory_simulation_get_active_research(
    const FactorySimulation *s) { return s==NULL?0U:s->research.active; }

FactoryResult factory_simulation_get_technology_progress(
    const FactorySimulation *s,FactoryTechnologyId id,
    FactoryTechnologyProgressInspection *out)
{
    size_t index=definition_index(id);
    const FactoryTechnologyDefinition *d;
    const FactoryTechnologyProgress *p;
    if (s==NULL || out==NULL) return FACTORY_RESULT_INVALID_ARGUMENT;
    if (index==SIZE_MAX) return FACTORY_RESULT_TECHNOLOGY_INVALID;
    d=factory_content_technology_at(index); p=&s->research.progress[index];
    *out=(FactoryTechnologyProgressInspection){id,p->completed_units,
        d->required_science_units,p->work_ticks,d->work_ticks_per_unit,
        p->science_committed,
        factory_simulation_is_technology_completed(s,id)};
    return FACTORY_RESULT_OK;
}

bool factory_simulation_has_unlock(const FactorySimulation *s,
    FactoryUnlockFlags unlock)
{
    FactoryUnlockFlags present=0U;
    if (s==NULL || unlock==0U || (unlock&~FACTORY_UNLOCK_ALL)!=0U) return false;
    for (size_t i=0U;i<factory_technology_definition_count();++i)
        if (factory_simulation_is_technology_completed(s,
                factory_content_technology_at(i)->id))
            present|=factory_content_technology_at(i)->unlock_flags;
    return (present&unlock)==unlock;
}

uint32_t factory_simulation_get_completed_technology_count(const FactorySimulation *s)
{
    uint32_t count=0U;
    if (s==NULL) return 0U;
    for (size_t i=0U;i<factory_technology_definition_count();++i)
        if (factory_simulation_is_technology_completed(s,
                factory_content_technology_at(i)->id)) ++count;
    return count;
}

uint32_t factory_simulation_get_research_science_quantity(const FactorySimulation *s)
{ return s==NULL?0U:s->research.science_quantity; }

FactoryResult factory_research_select(FactorySimulation *s,FactoryTechnologyId id)
{
    const FactoryTechnologyDefinition *d=factory_technology_definition_get(id);
    if (d==NULL) return FACTORY_RESULT_TECHNOLOGY_INVALID;
    if (factory_simulation_is_technology_completed(s,id))
        return FACTORY_RESULT_TECHNOLOGY_ALREADY_COMPLETED;
    for (size_t i=0U;i<d->prerequisite_count;++i)
        if (!factory_simulation_is_technology_completed(s,d->prerequisites[i]))
            return FACTORY_RESULT_TECHNOLOGY_PREREQUISITES_MISSING;
    s->research.active=id;
    factory_simulation_emit_event(s,(FactoryEvent){
        .type=FACTORY_EVENT_RESEARCH_SELECTED,.technology_id=id});
    return FACTORY_RESULT_OK;
}

FactoryResult factory_research_insert_science(FactorySimulation *s,uint32_t quantity)
{
    if (quantity==0U) return FACTORY_RESULT_INVALID_ARGUMENT;
    if (s->research.science_quantity>UINT32_MAX-quantity)
        return FACTORY_RESULT_RESEARCH_INVENTORY_OVERFLOW;
    s->research.science_quantity+=quantity;
    return FACTORY_RESULT_OK;
}

void factory_research_update(FactorySimulation *s)
{
    const FactoryTechnologyDefinition *d=factory_technology_definition_get(
        s->research.active);
    size_t index;
    FactoryTechnologyProgress *p;
    if (d==NULL) return;
    index=definition_index(d->id); p=&s->research.progress[index];
    if (!p->science_committed) {
        if (s->research.science_quantity<d->science_quantity_per_unit) return;
        s->research.science_quantity-=d->science_quantity_per_unit;
        p->science_committed=true;
    }
    if (p->work_ticks==UINT64_MAX) return;
    ++p->work_ticks;
    if (p->work_ticks<d->work_ticks_per_unit) return;
    p->work_ticks=0U; p->science_committed=false; ++p->completed_units;
    factory_simulation_emit_event(s,(FactoryEvent){
        .type=FACTORY_EVENT_RESEARCH_UNIT_COMPLETED,.technology_id=d->id,
        .item_type=d->science_item,.quantity=d->science_quantity_per_unit,
        .related_quantity=p->completed_units,
        .third_quantity=d->required_science_units});
    if (p->completed_units==d->required_science_units) {
        s->research.completed_bits|=UINT64_C(1)<<d->id;
        s->research.active=FACTORY_TECHNOLOGY_NONE;
        factory_simulation_emit_event(s,(FactoryEvent){
            .type=FACTORY_EVENT_TECHNOLOGY_COMPLETED,.technology_id=d->id,
            .quantity=p->completed_units});
    }
}

bool factory_research_state_valid(const FactoryResearchState *s)
{
    uint64_t valid_bits=0U;
    if (!factory_technology_definitions_validate(
            factory_content_get()->technologies,
            factory_technology_definition_count())) return false;
    for (size_t i=0U;i<factory_technology_definition_count();++i) {
        const FactoryTechnologyDefinition *d=factory_content_technology_at(i);
        const FactoryTechnologyProgress *p=&s->progress[i];
        valid_bits|=UINT64_C(1)<<d->id;
        if (p->completed_units>d->required_science_units
            || p->work_ticks>=d->work_ticks_per_unit
            || (!p->science_committed && p->work_ticks!=0U)
            || (((s->completed_bits&(UINT64_C(1)<<d->id))!=0U)
                !=(p->completed_units==d->required_science_units))) return false;
    }
    if ((s->completed_bits&~valid_bits)!=0U) return false;
    if (s->active!=0U) {
        const FactoryTechnologyDefinition *d=
            factory_technology_definition_get(s->active);
        if (d==NULL || (s->completed_bits&(UINT64_C(1)<<s->active))!=0U)
            return false;
        for (size_t i=0U;i<d->prerequisite_count;++i)
            if ((s->completed_bits&(UINT64_C(1)<<d->prerequisites[i]))==0U)
                return false;
    }
    return true;
}
