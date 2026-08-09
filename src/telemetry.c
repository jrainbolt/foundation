#include <foundation/telemetry.h>

#include <stdlib.h>
#include <string.h>

#define ITEM_COUNT ((size_t)FACTORY_ITEM_CONSTRUCTION_MATERIAL + 1U)
#define TELEMETRY_MAX_WINDOW 1200U

typedef struct {
    uint32_t received[ITEM_COUNT];
    uint32_t sent[ITEM_COUNT];
    uint32_t produced[ITEM_COUNT];
    uint8_t status;
    bool observed;
    bool occupied;
    bool blocked;
    bool holding;
    uint32_t cycles;
    uint32_t pickups;
    uint32_t drops;
    bool saturated;
} EntityTick;

typedef struct {
    uint32_t extracted[ITEM_COUNT];
    uint32_t produced[ITEM_COUNT];
    uint32_t transferred[ITEM_COUNT];
    uint32_t storage_in[ITEM_COUNT];
    uint32_t storage_out[ITEM_COUNT];
    bool saturated;
} GlobalTick;

typedef struct {
    FactoryEntityId id;
    FactoryEntityType type;
} EntityRecord;

struct FactoryTelemetry {
    FactoryTelemetryConfig config;
    EntityRecord *records;
    size_t record_count;
    EntityTick *entity_ticks;
    GlobalTick *global_ticks;
    FactoryPresentationSnapshot *presentation;
    uint64_t last_tick;
    uint32_t recorded_ticks;
    bool has_tick;
};

static bool item_valid(FactoryItemType item)
{return item>FACTORY_ITEM_NONE&&item<=FACTORY_ITEM_CONSTRUCTION_MATERIAL;}

void factory_telemetry_default_config(FactoryTelemetryConfig *out)
{if(out!=NULL)*out=(FactoryTelemetryConfig){60U,600U,FACTORY_TELEMETRY_DEFAULT_MAX_ENTITIES};}

static bool config_valid(const FactoryTelemetryConfig *c)
{return c!=NULL&&c->short_window_ticks>0U
    &&c->short_window_ticks<=c->long_window_ticks
    &&c->long_window_ticks<=TELEMETRY_MAX_WINDOW
    &&c->maximum_entities>0U&&c->maximum_entities<=4096U;}

FactoryTelemetry *factory_telemetry_create(const FactoryTelemetryConfig *config)
{
    FactoryTelemetry *t;size_t cells;
    if(!config_valid(config)||(size_t)config->long_window_ticks
        >SIZE_MAX/(size_t)config->maximum_entities)return NULL;
    cells=(size_t)config->long_window_ticks*config->maximum_entities;
    if(cells>SIZE_MAX/sizeof(EntityTick))return NULL;
    t=calloc(1U,sizeof(*t));if(t==NULL)return NULL;t->config=*config;
    t->records=calloc(config->maximum_entities,sizeof(*t->records));
    t->entity_ticks=calloc(cells,sizeof(*t->entity_ticks));
    t->global_ticks=calloc(config->long_window_ticks,sizeof(*t->global_ticks));
    t->presentation=factory_presentation_snapshot_create();
    if(t->records==NULL||t->entity_ticks==NULL||t->global_ticks==NULL
        ||t->presentation==NULL){factory_telemetry_destroy(t);return NULL;}
    return t;
}

void factory_telemetry_destroy(FactoryTelemetry *t)
{
    if(t==NULL)return;
    factory_presentation_snapshot_destroy(t->presentation);
    free(t->global_ticks);free(t->entity_ticks);free(t->records);free(t);
}

void factory_telemetry_clear(FactoryTelemetry *t)
{
    if(t==NULL)return;
    memset(t->records,0,(size_t)t->config.maximum_entities*sizeof(*t->records));
    memset(t->entity_ticks,0,(size_t)t->config.long_window_ticks
        *t->config.maximum_entities*sizeof(*t->entity_ticks));
    memset(t->global_ticks,0,(size_t)t->config.long_window_ticks
        *sizeof(*t->global_ticks));
    t->record_count=0U;t->last_tick=0U;t->recorded_ticks=0U;t->has_tick=false;
    factory_presentation_snapshot_clear(t->presentation);
}

static size_t find_record(const FactoryTelemetry *t,FactoryEntityId id)
{for(size_t i=0U;i<t->record_count;++i)if(t->records[i].id==id)return i;return SIZE_MAX;}
static size_t ensure_record(FactoryTelemetry *t,FactoryEntityId id,
    FactoryEntityType type)
{
    size_t i=find_record(t,id);if(i!=SIZE_MAX){if(type!=FACTORY_ENTITY_TYPE_NONE)t->records[i].type=type;return i;}
    if(t->record_count>=t->config.maximum_entities)return SIZE_MAX;
    i=t->record_count++;t->records[i]=(EntityRecord){id,type};return i;
}

static FactoryEntityType type_for_id(const FactoryTelemetry *t,
    FactoryEntityId id)
{size_t i=find_record(t,id);return i==SIZE_MAX?FACTORY_ENTITY_TYPE_NONE:t->records[i].type;}

static bool capacity_sufficient(const FactoryTelemetry *t,
    const FactoryPresentationSnapshot *p,const FactorySimulation *s)
{
    size_t new_count=0U,entity_count=factory_presentation_snapshot_get_entity_count(p);
    for(size_t i=0U;i<entity_count;++i){const FactoryPresentationEntity *e=
        factory_presentation_snapshot_get_entity(p,i);if(find_record(t,e->entity_id)==SIZE_MAX)++new_count;}
    for(size_t i=0U;i<factory_simulation_get_event_count(s);++i){
        const FactoryEvent *e=factory_simulation_get_event(s,i);FactoryEntityId ids[2]={e->entity_id,e->related_entity_id};
        for(size_t k=0U;k<2U;++k){bool seen=false;if(ids[k]==0U||find_record(t,ids[k])!=SIZE_MAX)continue;
            for(size_t j=0U;j<entity_count;++j)if(factory_presentation_snapshot_get_entity(p,j)->entity_id==ids[k])seen=true;
            for(size_t j=0U;j<i&&!seen;++j){const FactoryEvent *old=factory_simulation_get_event(s,j);
                if(old->entity_id==ids[k]||old->related_entity_id==ids[k])seen=true;}
            if(!seen)++new_count;
        }
    }
    return new_count<=t->config.maximum_entities-t->record_count;
}

static EntityTick *tick_for(FactoryTelemetry *t,size_t slot,size_t record)
{return &t->entity_ticks[slot*(size_t)t->config.maximum_entities+record];}
static void add32(uint32_t *value,uint32_t add,bool *saturated)
{if(UINT32_MAX-*value<add){*value=UINT32_MAX;*saturated=true;}else *value+=add;}

FactoryTelemetryResult factory_telemetry_observe_step(FactoryTelemetry *t,
    const FactorySimulation *simulation)
{
    uint64_t tick;size_t slot,entity_count;
    if(t==NULL||simulation==NULL)return FACTORY_TELEMETRY_RESULT_INVALID_ARGUMENT;
    tick=factory_simulation_get_tick(simulation);
    if(tick==0U)return FACTORY_TELEMETRY_RESULT_NON_SEQUENTIAL;
    if(t->has_tick&&tick==t->last_tick)return FACTORY_TELEMETRY_RESULT_DUPLICATE;
    if(t->has_tick&&tick!=t->last_tick+1U)return FACTORY_TELEMETRY_RESULT_NON_SEQUENTIAL;
    if(factory_presentation_snapshot_rebuild(t->presentation,simulation)
        !=FACTORY_RESULT_OK)return FACTORY_TELEMETRY_RESULT_OUT_OF_MEMORY;
    for(size_t i=0U;i<factory_simulation_get_event_count(simulation);++i){
        const FactoryEvent *e=factory_simulation_get_event(simulation,i);
        if(e->tick+1U!=tick)return FACTORY_TELEMETRY_RESULT_NON_SEQUENTIAL;
    }
    if(!capacity_sufficient(t,t->presentation,simulation))
        return FACTORY_TELEMETRY_RESULT_ENTITY_CAPACITY;
    slot=(size_t)(tick%t->config.long_window_ticks);
    memset(&t->entity_ticks[slot*(size_t)t->config.maximum_entities],0,
        (size_t)t->config.maximum_entities*sizeof(EntityTick));
    memset(&t->global_ticks[slot],0,sizeof(GlobalTick));
    entity_count=factory_presentation_snapshot_get_entity_count(t->presentation);
    for(size_t i=0U;i<entity_count;++i){
        const FactoryPresentationEntity *e=factory_presentation_snapshot_get_entity(t->presentation,i);
        size_t record=ensure_record(t,e->entity_id,e->entity_type);EntityTick *v=tick_for(t,slot,record);
        v->observed=true;v->status=(uint8_t)e->status;
        if(e->entity_type==FACTORY_ENTITY_TYPE_BELT){v->occupied=e->data.belt.quantity!=0U;
            v->blocked=v->occupied&&e->data.belt.movement_progress>=e->data.belt.movement_duration;}
        else if(e->entity_type==FACTORY_ENTITY_TYPE_SPLITTER){v->occupied=e->data.splitter.quantity!=0U;}
        else if(e->entity_type==FACTORY_ENTITY_TYPE_INSERTER){v->holding=e->data.inserter.held_quantity!=0U;}
    }
    for(size_t i=0U;i<factory_simulation_get_event_count(simulation);++i){
        const FactoryEvent *e=factory_simulation_get_event(simulation,i);
        if(e->type==FACTORY_EVENT_PRODUCTION_COMPLETED&&item_valid(e->item_type)){
            size_t r=ensure_record(t,e->entity_id,FACTORY_ENTITY_TYPE_NONE);
            add32(&t->global_ticks[slot].produced[e->item_type],e->quantity,
                &t->global_ticks[slot].saturated);
            add32(&tick_for(t,slot,r)->produced[e->item_type],e->quantity,
                &tick_for(t,slot,r)->saturated);
            add32(&tick_for(t,slot,r)->cycles,1U,&tick_for(t,slot,r)->saturated);
            if(type_for_id(t,e->entity_id)==FACTORY_ENTITY_TYPE_EXTRACTOR)
                add32(&t->global_ticks[slot].extracted[e->item_type],e->quantity,
                    &t->global_ticks[slot].saturated);
        } else if(e->type==FACTORY_EVENT_ITEM_TRANSFERRED&&item_valid(e->item_type)){
            size_t source=ensure_record(t,e->entity_id,FACTORY_ENTITY_TYPE_NONE);
            size_t destination=ensure_record(t,e->related_entity_id,FACTORY_ENTITY_TYPE_NONE);
            add32(&t->global_ticks[slot].transferred[e->item_type],e->quantity,
                &t->global_ticks[slot].saturated);
            add32(&tick_for(t,slot,source)->sent[e->item_type],e->quantity,
                &tick_for(t,slot,source)->saturated);
            add32(&tick_for(t,slot,destination)->received[e->item_type],e->quantity,
                &tick_for(t,slot,destination)->saturated);
            if(type_for_id(t,e->entity_id)==FACTORY_ENTITY_TYPE_STORAGE)
                add32(&t->global_ticks[slot].storage_out[e->item_type],e->quantity,
                    &t->global_ticks[slot].saturated);
            if(type_for_id(t,e->related_entity_id)==FACTORY_ENTITY_TYPE_STORAGE)
                add32(&t->global_ticks[slot].storage_in[e->item_type],e->quantity,
                    &t->global_ticks[slot].saturated);
            if(type_for_id(t,e->related_entity_id)==FACTORY_ENTITY_TYPE_INSERTER)
                add32(&tick_for(t,slot,destination)->pickups,e->quantity,
                    &tick_for(t,slot,destination)->saturated);
            if(type_for_id(t,e->entity_id)==FACTORY_ENTITY_TYPE_INSERTER)
                add32(&tick_for(t,slot,source)->drops,e->quantity,
                    &tick_for(t,slot,source)->saturated);
        }
    }
    t->last_tick=tick;t->has_tick=true;
    if(t->recorded_ticks<t->config.long_window_ticks)++t->recorded_ticks;
    return FACTORY_TELEMETRY_RESULT_OK;
}

static uint32_t window_ticks(const FactoryTelemetry *t,FactoryTelemetryWindow w)
{
    uint32_t requested=w==FACTORY_TELEMETRY_WINDOW_CURRENT_TICK?1U:
        (w==FACTORY_TELEMETRY_WINDOW_SHORT?t->config.short_window_ticks:
        (w==FACTORY_TELEMETRY_WINDOW_LONG?t->config.long_window_ticks:0U));
    return requested<t->recorded_ticks?requested:t->recorded_ticks;
}
static void add64(uint64_t *value,uint64_t add,bool *saturated)
{if(UINT64_MAX-*value<add){*value=UINT64_MAX;*saturated=true;}else *value+=add;}

static int64_t net_flow(uint64_t received,uint64_t sent,bool *saturated)
{
    if(received>=sent){uint64_t difference=received-sent;
        if(difference>(uint64_t)INT64_MAX){*saturated=true;return INT64_MAX;}
        return (int64_t)difference;}
    {uint64_t difference=sent-received;
        uint64_t minimum_magnitude=(uint64_t)INT64_MAX+UINT64_C(1);
        if(difference>minimum_magnitude){*saturated=true;return INT64_MIN;}
        if(difference==minimum_magnitude)return INT64_MIN;
        return -(int64_t)difference;}
}

bool factory_telemetry_get_item_metrics(const FactoryTelemetry *t,
    FactoryItemType item,FactoryTelemetryWindow window,FactoryTelemetryItemMetrics *out)
{
    uint32_t ticks;if(t==NULL||out==NULL||!item_valid(item)||(ticks=window_ticks(t,window))==0U){if(out!=NULL)*out=(FactoryTelemetryItemMetrics){0};return false;}
    *out=(FactoryTelemetryItemMetrics){.observed_ticks=ticks};
    for(uint32_t age=0U;age<ticks;++age){size_t slot=(size_t)((t->last_tick-age)%t->config.long_window_ticks);const GlobalTick *v=&t->global_ticks[slot];
        if(v->saturated)out->saturated=true;
        add64(&out->extracted_quantity,v->extracted[item],&out->saturated);add64(&out->produced_quantity,v->produced[item],&out->saturated);
        add64(&out->transferred_quantity,v->transferred[item],&out->saturated);add64(&out->storage_inflow,v->storage_in[item],&out->saturated);add64(&out->storage_outflow,v->storage_out[item],&out->saturated);}
    return true;
}

bool factory_telemetry_get_entity_item_metrics(const FactoryTelemetry *t,
    FactoryEntityId id,FactoryItemType item,FactoryTelemetryWindow window,
    FactoryTelemetryEntityItemMetrics *out)
{
    size_t record;uint32_t ticks;if(t==NULL||out==NULL||!item_valid(item)||(record=find_record(t,id))==SIZE_MAX||(ticks=window_ticks(t,window))==0U){if(out!=NULL)*out=(FactoryTelemetryEntityItemMetrics){0};return false;}
    *out=(FactoryTelemetryEntityItemMetrics){.observed_ticks=ticks};
    for(uint32_t age=0U;age<ticks;++age){size_t slot=(size_t)((t->last_tick-age)%t->config.long_window_ticks);const EntityTick*v=&t->entity_ticks[slot*(size_t)t->config.maximum_entities+record];
        if(v->saturated)out->saturated=true;
        add64(&out->received_quantity,v->received[item],&out->saturated);add64(&out->sent_quantity,v->sent[item],&out->saturated);add64(&out->produced_quantity,v->produced[item],&out->saturated);}
    out->net_flow=net_flow(out->received_quantity,out->sent_quantity,
        &out->saturated);
    return true;
}

bool factory_telemetry_get_entity_metrics(const FactoryTelemetry *t,
    FactoryEntityId id,FactoryTelemetryWindow window,FactoryTelemetryEntityMetrics *out)
{
    size_t record;uint32_t ticks;if(t==NULL||out==NULL||(record=find_record(t,id))==SIZE_MAX||(ticks=window_ticks(t,window))==0U){if(out!=NULL)*out=(FactoryTelemetryEntityMetrics){0};return false;}
    *out=(FactoryTelemetryEntityMetrics){.entity_id=id,.entity_type=t->records[record].type};
    for(uint32_t age=0U;age<ticks;++age){size_t slot=(size_t)((t->last_tick-age)%t->config.long_window_ticks);const EntityTick*v=&t->entity_ticks[slot*(size_t)t->config.maximum_entities+record];
        if(v->saturated)out->saturated=true;
        if(v->observed)++out->observed_ticks;
        if(v->status==FACTORY_PRESENTATION_MACHINE_STATUS_WORKING)++out->working_ticks;
        else if(v->status==FACTORY_PRESENTATION_MACHINE_STATUS_BLOCKED_INPUT)++out->blocked_input_ticks;
        else if(v->status==FACTORY_PRESENTATION_MACHINE_STATUS_BLOCKED_OUTPUT)++out->blocked_output_ticks;
        else if(v->status==FACTORY_PRESENTATION_MACHINE_STATUS_UNPOWERED)++out->unpowered_ticks;
        else if(v->status==FACTORY_PRESENTATION_MACHINE_STATUS_IDLE)++out->idle_ticks;
        else if(v->status==FACTORY_PRESENTATION_MACHINE_STATUS_DEPLETED_RESOURCE)++out->depleted_ticks;
        if(v->occupied)++out->occupied_ticks;else if(v->observed&&(out->entity_type==FACTORY_ENTITY_TYPE_BELT||out->entity_type==FACTORY_ENTITY_TYPE_SPLITTER))++out->empty_ticks;
        if(v->blocked)++out->blocked_ticks;
        if(v->holding)++out->holding_ticks;
        add64(&out->completed_cycles,v->cycles,&out->saturated);
        add64(&out->pickups,v->pickups,&out->saturated);
        add64(&out->drops,v->drops,&out->saturated);
        for(size_t item=1U;item<ITEM_COUNT;++item){add64(&out->received_quantity,v->received[item],&out->saturated);add64(&out->sent_quantity,v->sent[item],&out->saturated);}
    }
    out->net_flow=net_flow(out->received_quantity,out->sent_quantity,
        &out->saturated);
    return true;
}

uint64_t factory_telemetry_get_last_observed_tick(const FactoryTelemetry *t)
{return t==NULL||!t->has_tick?0U:t->last_tick;}
const FactoryTelemetryConfig *factory_telemetry_get_config(const FactoryTelemetry *t)
{return t==NULL?NULL:&t->config;}
