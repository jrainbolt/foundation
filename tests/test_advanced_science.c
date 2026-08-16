#include "foundation/content.h"
#include "foundation/snapshot.h"
#include "foundation/telemetry.h"
#include "logistics_endpoint_internal.h"
#include "simulation_internal.h"
#include "power_fixture.h"
#include "tick_preflight_internal.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr,"FAIL %s:%d: %s\n", \
    __FILE__,__LINE__,#c); ++failures; } } while (false)

static void submit(FactorySimulation *s,FactoryCommand c)
{CHECK(factory_simulation_submit_command(s,&c)==FACTORY_RESULT_OK);}

static void ticks(FactorySimulation *s,uint32_t count)
{for(uint32_t i=0U;i<count;++i)CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);}

static void ticks_observe(FactorySimulation *s,FactoryTelemetry *telemetry,
    uint32_t count)
{
    for(uint32_t i=0U;i<count;++i){
        CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
        CHECK(factory_telemetry_observe_step(telemetry,s)
            ==FACTORY_TELEMETRY_RESULT_OK);
    }
}

static FactoryEntityId place_and_get(FactorySimulation *s,FactoryCommand command)
{
    submit(s,command);
    ticks(s,1U);
    const FactoryCommandResult *result=factory_simulation_get_command_result(s,0U);
    if(result==NULL||result->result!=FACTORY_RESULT_OK)
        fprintf(stderr,"placement command %u failed: %d\n",
            (unsigned)command.type,result!=NULL?(int)result->result:-1);
    CHECK(result!=NULL&&result->result==FACTORY_RESULT_OK);
    return result!=NULL?result->entity_id:0U;
}

static bool snapshots_equal(const FactorySimulation *a,
    const FactorySimulation *b)
{
    FactorySnapshotBuffer first={0},second={0};
    FactoryResult first_result=factory_simulation_create_snapshot(a,&first);
    FactoryResult second_result=factory_simulation_create_snapshot(b,&second);
    bool equal=first_result==FACTORY_RESULT_OK
        &&second_result==FACTORY_RESULT_OK
        &&first.size==second.size
        &&memcmp(first.data,second.data,first.size)==0;
    static bool reported=false;
    if(!equal&&!reported){
        size_t difference=0U;
        while(difference<first.size&&difference<second.size
            &&first.data[difference]==second.data[difference])++difference;
        fprintf(stderr,"snapshot continuation differs: result=%d/%d size=%zu/%zu offset=%zu tick=%llu/%llu\n",
            (int)first_result,(int)second_result,first.size,second.size,
            difference,(unsigned long long)factory_simulation_get_tick(a),
            (unsigned long long)factory_simulation_get_tick(b));
        reported=true;
    }
    factory_snapshot_buffer_destroy(&first);
    factory_snapshot_buffer_destroy(&second);
    return equal;
}

static void test_catalog_and_physical_chain(void)
{
    FactoryWorld *world=factory_world_create(16U,6U);
    FactorySimulation *s=factory_simulation_create_with_construction_units(
        world,UINT32_MAX);
    const FactoryRefineryRecipeDefinition *steel=
        factory_content_refinery_recipe_get(FACTORY_RECIPE_STEEL);
    const FactoryAssemblerRecipe *component=
        factory_content_assembler_recipe_get(
            FACTORY_ASSEMBLER_RECIPE_ADVANCED_COMPONENT);
    const FactoryAssemblerRecipe *science=
        factory_content_assembler_recipe_get(
            FACTORY_ASSEMBLER_RECIPE_ADVANCED_SCIENCE);
    FactoryRefinery refinery;
    FactoryAssembler assembler;
    FactoryResearchLabInspection lab;
    FactorySnapshotBuffer bytes={0};
    FactorySnapshotBuffer before_observe={0},after_observe={0};
    FactorySimulation *loaded=NULL;
    FactoryTelemetryConfig telemetry_config={16U,1024U,64U};
    FactoryTelemetry *telemetry=factory_telemetry_create(&telemetry_config);

    CHECK(steel!=NULL&&steel->recipe.input_item==FACTORY_ITEM_IRON_PLATE
        &&steel->recipe.input_amount==2U
        &&steel->recipe.secondary_input_item==FACTORY_ITEM_COAL
        &&steel->recipe.secondary_input_amount==1U
        &&steel->recipe.output_item==FACTORY_ITEM_STEEL);
    CHECK(component!=NULL&&component->input_items[0]==FACTORY_ITEM_STEEL
        &&component->input_items[1]==FACTORY_ITEM_COPPER_WIRE);
    CHECK(science!=NULL
        &&science->input_items[0]==FACTORY_ITEM_ADVANCED_COMPONENT
        &&science->input_items[1]==FACTORY_ITEM_ELECTRONIC_COMPONENT);

    s->research.completed_bits|=
        (UINT64_C(1)<<FACTORY_TECHNOLOGY_BASIC_AUTOMATION)
        |(UINT64_C(1)<<FACTORY_TECHNOLOGY_FLUID_HANDLING);
    s->research.progress[0].completed_units=2U;
    s->research.progress[1].completed_units=2U;
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_REFINERY,
        {.place_refinery={1,1,FACTORY_DIRECTION_WEST,FACTORY_DIRECTION_EAST}}});
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_ASSEMBLER,
        {.place_assembler={4,1,FACTORY_DIRECTION_EAST}}});
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_ASSEMBLER,
        {.place_assembler={7,1,FACTORY_DIRECTION_EAST}}});
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_RESEARCH_LAB,
        {.place_research_lab={10,1}}});
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_ASSEMBLER,
        {.place_assembler={12,1,FACTORY_DIRECTION_EAST}}});
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_ASSEMBLER,
        {.place_assembler={15,1,FACTORY_DIRECTION_EAST}}});
    CHECK(factory_test_submit_power_row(s,16U,4U));
    ticks_observe(s,telemetry,1U);
    submit(s,(FactoryCommand){FACTORY_COMMAND_SET_REFINERY_RECIPE,
        {.set_refinery_recipe={1U,FACTORY_RECIPE_STEEL}}});
    submit(s,(FactoryCommand){FACTORY_COMMAND_SET_ASSEMBLER_RECIPE,
        {.set_assembler_recipe={2U,
            FACTORY_ASSEMBLER_RECIPE_ADVANCED_COMPONENT}}});
    submit(s,(FactoryCommand){FACTORY_COMMAND_SET_ASSEMBLER_RECIPE,
        {.set_assembler_recipe={3U,
            FACTORY_ASSEMBLER_RECIPE_ADVANCED_SCIENCE}}});
    submit(s,(FactoryCommand){FACTORY_COMMAND_SET_ASSEMBLER_RECIPE,
        {.set_assembler_recipe={5U,
            FACTORY_ASSEMBLER_RECIPE_COPPER_WIRE}}});
    submit(s,(FactoryCommand){FACTORY_COMMAND_SET_ASSEMBLER_RECIPE,
        {.set_assembler_recipe={6U,
            FACTORY_ASSEMBLER_RECIPE_ELECTRONIC_COMPONENT}}});
    ticks_observe(s,telemetry,1U);
    for(uint32_t cycle=0U;cycle<6U;++cycle){
        for(uint32_t i=0U;i<2U;++i)CHECK(factory_logistics_endpoint_insert(s,
            (FactoryLogisticsEndpoint){1U,FACTORY_LOGISTICS_SLOT_INPUT},
            FACTORY_ITEM_IRON_PLATE)==FACTORY_LOGISTICS_RESULT_OK);
        CHECK(factory_logistics_endpoint_insert(s,
            (FactoryLogisticsEndpoint){1U,FACTORY_LOGISTICS_SLOT_INPUT},
            FACTORY_ITEM_COAL)==FACTORY_LOGISTICS_RESULT_OK);
        CHECK(factory_logistics_endpoint_insert(s,
            (FactoryLogisticsEndpoint){5U,
                FACTORY_LOGISTICS_SLOT_ASSEMBLER_INPUT_0},
            FACTORY_ITEM_COPPER_PLATE)==FACTORY_LOGISTICS_RESULT_OK);
        CHECK(factory_logistics_endpoint_insert(s,
            (FactoryLogisticsEndpoint){6U,
                FACTORY_LOGISTICS_SLOT_ASSEMBLER_INPUT_0},
            FACTORY_ITEM_IRON_PLATE)==FACTORY_LOGISTICS_RESULT_OK);
        CHECK(factory_logistics_endpoint_insert(s,
            (FactoryLogisticsEndpoint){6U,
                FACTORY_LOGISTICS_SLOT_ASSEMBLER_INPUT_1},
            FACTORY_ITEM_COPPER_PLATE)==FACTORY_LOGISTICS_RESULT_OK);
        ticks_observe(s,telemetry,20U);
        CHECK(factory_simulation_get_refinery(s,1U,&refinery)
            &&refinery.output_item==FACTORY_ITEM_STEEL
            &&refinery.output_amount==1U);
        CHECK(factory_logistics_endpoint_transfer(s,
            (FactoryLogisticsEndpoint){1U,FACTORY_LOGISTICS_SLOT_OUTPUT},
            (FactoryLogisticsEndpoint){2U,
                FACTORY_LOGISTICS_SLOT_ASSEMBLER_INPUT_0},
            FACTORY_ITEM_STEEL)==FACTORY_LOGISTICS_RESULT_OK);
        for(uint32_t i=0U;i<2U;++i)CHECK(factory_logistics_endpoint_transfer(s,
            (FactoryLogisticsEndpoint){5U,FACTORY_LOGISTICS_SLOT_OUTPUT},
            (FactoryLogisticsEndpoint){2U,
                FACTORY_LOGISTICS_SLOT_ASSEMBLER_INPUT_1},
            FACTORY_ITEM_COPPER_WIRE)==FACTORY_LOGISTICS_RESULT_OK);
        ticks_observe(s,telemetry,20U);
        CHECK(factory_simulation_get_assembler(s,2U,&assembler)
            &&assembler.output_item==FACTORY_ITEM_ADVANCED_COMPONENT);
        CHECK(factory_logistics_endpoint_transfer(s,
            (FactoryLogisticsEndpoint){2U,FACTORY_LOGISTICS_SLOT_OUTPUT},
            (FactoryLogisticsEndpoint){3U,
                FACTORY_LOGISTICS_SLOT_ASSEMBLER_INPUT_0},
            FACTORY_ITEM_ADVANCED_COMPONENT)==FACTORY_LOGISTICS_RESULT_OK);
        CHECK(factory_logistics_endpoint_transfer(s,
            (FactoryLogisticsEndpoint){6U,FACTORY_LOGISTICS_SLOT_OUTPUT},
            (FactoryLogisticsEndpoint){3U,
                FACTORY_LOGISTICS_SLOT_ASSEMBLER_INPUT_1},
            FACTORY_ITEM_ELECTRONIC_COMPONENT)==FACTORY_LOGISTICS_RESULT_OK);
        ticks_observe(s,telemetry,25U);
        CHECK(factory_simulation_get_assembler(s,3U,&assembler)
            &&assembler.output_item==FACTORY_ITEM_ADVANCED_SCIENCE);
        CHECK(factory_logistics_endpoint_transfer(s,
            (FactoryLogisticsEndpoint){3U,FACTORY_LOGISTICS_SLOT_OUTPUT},
            (FactoryLogisticsEndpoint){4U,
                FACTORY_LOGISTICS_SLOT_RESEARCH_LAB_INPUT},
            FACTORY_ITEM_ADVANCED_SCIENCE)==FACTORY_LOGISTICS_RESULT_OK);
        if(cycle==0U)CHECK(factory_logistics_endpoint_insert(s,
            (FactoryLogisticsEndpoint){4U,
                FACTORY_LOGISTICS_SLOT_RESEARCH_LAB_INPUT},
            FACTORY_ITEM_BASIC_SCIENCE)
            ==FACTORY_LOGISTICS_RESULT_INCOMPATIBLE_ITEM);
    }
    CHECK(factory_simulation_get_research_lab(s,4U,&lab)==FACTORY_RESULT_OK
        &&lab.science_item==FACTORY_ITEM_ADVANCED_SCIENCE
        &&lab.science_quantity==6U);
    submit(s,(FactoryCommand){FACTORY_COMMAND_SELECT_RESEARCH,
        {.select_research={FACTORY_TECHNOLOGY_ADVANCED_MANUFACTURING}}});
    ticks_observe(s,telemetry,3U);
    CHECK(factory_simulation_create_snapshot(s,&bytes)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_load_snapshot(bytes.data,bytes.size,&loaded)
        ==FACTORY_RESULT_OK);
    for(uint32_t i=0U;i<6U;++i){
        FactorySnapshotBuffer continued={0},loaded_continued={0};
        ticks_observe(s,telemetry,1U);
        ticks(loaded,1U);
        CHECK(factory_simulation_create_snapshot(s,&continued)
            ==FACTORY_RESULT_OK);
        CHECK(factory_simulation_create_snapshot(loaded,&loaded_continued)
            ==FACTORY_RESULT_OK);
        CHECK(continued.size==loaded_continued.size
            &&memcmp(continued.data,loaded_continued.data,continued.size)==0);
        CHECK(factory_simulation_get_event_count(s)
            ==factory_simulation_get_event_count(loaded));
        factory_snapshot_buffer_destroy(&continued);
        factory_snapshot_buffer_destroy(&loaded_continued);
    }
    CHECK(factory_simulation_is_technology_completed(s,
        FACTORY_TECHNOLOGY_ADVANCED_MANUFACTURING));
    CHECK(factory_simulation_is_technology_completed(loaded,
        FACTORY_TECHNOLOGY_ADVANCED_MANUFACTURING));
    CHECK(factory_simulation_has_unlock(s,
        FACTORY_UNLOCK_ADVANCED_MANUFACTURING));
    CHECK(factory_simulation_get_research_lab(s,4U,&lab)==FACTORY_RESULT_OK
        &&lab.science_item==FACTORY_ITEM_NONE&&lab.science_quantity==0U);
    {
        uint32_t completions=0U;
        for(size_t i=0U;i<factory_simulation_get_event_count(s);++i)
            if(factory_simulation_get_event(s,i)->type
                ==FACTORY_EVENT_TECHNOLOGY_COMPLETED)++completions;
        CHECK(completions==1U);
        printf("Advanced Manufacturing completed at tick %llu\n",
            (unsigned long long)factory_simulation_get_tick(s));
    }
    {
        FactoryTelemetryItemMetrics steel_metrics,component_metrics;
        FactoryTelemetryItemMetrics science_metrics;
        CHECK(factory_telemetry_get_item_metrics(telemetry,FACTORY_ITEM_STEEL,
            FACTORY_TELEMETRY_WINDOW_LONG,&steel_metrics));
        CHECK(factory_telemetry_get_item_metrics(telemetry,
            FACTORY_ITEM_ADVANCED_COMPONENT,FACTORY_TELEMETRY_WINDOW_LONG,
            &component_metrics));
        CHECK(factory_telemetry_get_item_metrics(telemetry,
            FACTORY_ITEM_ADVANCED_SCIENCE,FACTORY_TELEMETRY_WINDOW_LONG,
            &science_metrics));
        CHECK(steel_metrics.produced_quantity==6U);
        CHECK(component_metrics.produced_quantity==6U);
        CHECK(science_metrics.produced_quantity==6U);
    }
    CHECK(factory_simulation_create_snapshot(s,&before_observe)
        ==FACTORY_RESULT_OK);
    CHECK(factory_telemetry_get_last_observed_tick(telemetry)
        ==factory_simulation_get_tick(s));
    CHECK(factory_simulation_create_snapshot(s,&after_observe)
        ==FACTORY_RESULT_OK);
    CHECK(before_observe.size==after_observe.size
        &&memcmp(before_observe.data,after_observe.data,before_observe.size)==0);
    CHECK(factory_simulation_get_research_lab(loaded,4U,&lab)==FACTORY_RESULT_OK
        &&lab.science_item==FACTORY_ITEM_NONE&&lab.science_quantity==0U);
    factory_snapshot_buffer_destroy(&bytes);
    factory_snapshot_buffer_destroy(&before_observe);
    factory_snapshot_buffer_destroy(&after_observe);
    factory_simulation_destroy(loaded);
    factory_telemetry_destroy(telemetry);
    factory_simulation_destroy(s);factory_world_destroy(world);
}

static void test_finite_coal_depletion(void)
{
    FactoryWorld *world=factory_world_create(5U,4U);
    FactorySimulation *s;
    FactoryExtractor extractor;
    unsigned depletion_events=0U;
    CHECK(factory_world_add_resource(world,1,1,FACTORY_RESOURCE_COAL,1U)
        ==FACTORY_RESULT_OK);
    s=factory_simulation_create_with_construction_units(world,UINT32_MAX);
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor={1,1,FACTORY_DIRECTION_EAST}}});
    CHECK(factory_test_submit_power_pair(s,2,1,3,1));
    ticks(s,20U);
    CHECK(factory_simulation_get_extractor(s,1U,&extractor)
        &&extractor.produced_item==FACTORY_ITEM_COAL
        &&extractor.output_item==FACTORY_ITEM_COAL);
    CHECK(factory_world_get_tile(world,1,1)->resource_amount==0U);
    for(size_t i=0U;i<factory_simulation_get_event_count(s);++i)
        if(factory_simulation_get_event(s,i)->type
            ==FACTORY_EVENT_RESOURCE_DEPLETED)++depletion_events;
    CHECK(depletion_events==1U);
    ticks(s,20U);
    CHECK(factory_world_get_tile(world,1,1)->resource_amount==0U);
    factory_simulation_destroy(s);factory_world_destroy(world);
}

static uint32_t coal_and_steel_accounting(const FactorySimulation *s,
    int32_t coal_x,int32_t coal_y,FactoryEntityId extractor_id,
    FactoryEntityId coal_belt_id,FactoryEntityId load_inserter_id,
    FactoryEntityId load_station_id,
    FactoryEntityId wagon_id,FactoryEntityId unload_station_id,
    FactoryEntityId unload_inserter_id,FactoryEntityId refinery_id,
    FactoryEntityId output_inserter_id,FactoryEntityId steel_storage_id)
{
    const FactoryWorld *world=factory_simulation_get_world(s);
    const FactoryTile *deposit=factory_world_get_tile(world,coal_x,coal_y);
    FactoryExtractor extractor;
    FactoryBelt belt;
    FactoryInserter inserter;
    FactoryRailStationInspection station;
    FactoryCargoWagonInspection wagon;
    FactoryRefinery refinery;
    FactoryStorage storage;
    uint32_t total=deposit!=NULL?deposit->resource_amount:0U;
    CHECK(factory_simulation_get_extractor(s,extractor_id,&extractor));
    if(extractor.output_item==FACTORY_ITEM_COAL)total+=extractor.output_amount;
    CHECK(factory_simulation_get_belt(s,coal_belt_id,&belt));
    if(belt.item==FACTORY_ITEM_COAL)++total;
    CHECK(factory_simulation_get_inserter(s,load_inserter_id,&inserter));
    if(inserter.held_item==FACTORY_ITEM_COAL)total+=inserter.held_amount;
    CHECK(factory_simulation_get_rail_station(s,load_station_id,&station));
    if(station.configured_item==FACTORY_ITEM_COAL)
        total+=station.freight_quantity;
    CHECK(factory_simulation_get_cargo_wagon(s,wagon_id,&wagon));
    if(wagon.cargo_item==FACTORY_ITEM_COAL)total+=wagon.cargo_quantity;
    CHECK(factory_simulation_get_rail_station(s,unload_station_id,&station));
    if(station.configured_item==FACTORY_ITEM_COAL)
        total+=station.freight_quantity;
    CHECK(factory_simulation_get_inserter(s,unload_inserter_id,&inserter));
    if(inserter.held_item==FACTORY_ITEM_COAL)total+=inserter.held_amount;
    CHECK(factory_simulation_get_refinery(s,refinery_id,&refinery));
    if(refinery.secondary_input_item==FACTORY_ITEM_COAL)
        total+=refinery.secondary_input_amount;
    if(refinery.processing)++total;
    if(refinery.output_item==FACTORY_ITEM_STEEL)total+=refinery.output_amount;
    CHECK(factory_simulation_get_inserter(s,output_inserter_id,&inserter));
    if(inserter.held_item==FACTORY_ITEM_STEEL)total+=inserter.held_amount;
    CHECK(factory_simulation_get_storage(s,steel_storage_id,&storage));
    total+=storage.steel_amount;
    if(storage.output_occupied&&storage.output_item==FACTORY_ITEM_STEEL)++total;
    return total;
}

static void test_generated_coal_autonomous_freight_to_steel(void)
{
    FactoryWorldGenerationConfig generation;
    FactoryWorld *world=factory_world_create_with_seed(64U,48U,UINT64_C(6));
    int32_t coal_x=0,coal_y=0;
    uint32_t best=UINT32_MAX,initial_coal=0U;
    factory_world_generation_default_config(&generation);
    generation.water_threshold=0U;
    generation.rock_threshold=0U;
    CHECK(factory_world_generate(world,&generation)==FACTORY_RESULT_OK);
    const int32_t start_x=factory_world_get_start_x(world);
    const int32_t start_y=factory_world_get_start_y(world);
    for(int32_t y=8;y<38;++y)for(int32_t x=8;x<56;++x){
        const FactoryTile *tile=factory_world_get_tile(world,x,y);
        uint32_t distance=(uint32_t)(x>start_x?x-start_x:start_x-x)
            +(uint32_t)(y>start_y?y-start_y:start_y-y);
        if(tile->resource==FACTORY_RESOURCE_COAL&&distance>11U
            &&distance<best){
            best=distance;coal_x=x;coal_y=y;
            initial_coal=tile->resource_amount;
        }
    }
    CHECK(best!=UINT32_MAX&&initial_coal!=0U);
    printf("generated freight Coal: start=(%d,%d) patch=(%d,%d) distance=%u quantity=%u\n",
        start_x,start_y,coal_x,coal_y,best,initial_coal);

    FactorySimulation *s=factory_simulation_create_with_construction_units(
        world,FACTORY_CONSTRUCTION_COST_CONSTRUCTION_DEPOT+500U);
    s->fixture_initial_generator_fuel=FACTORY_TEST_GENERATOR_FUEL_QUANTITY;
    FactoryEntityId depot=place_and_get(s,(FactoryCommand){
        FACTORY_COMMAND_PLACE_CONSTRUCTION_DEPOT,
        {.place_construction_depot={coal_x+2,coal_y+5}}});
    CHECK(s->construction_depots.items[0].entity_id==depot
        &&s->construction_depots.items[0].material_quantity==500U);

    const int32_t ox=coal_x-4,oy=coal_y+1;
    FactoryEntityId top[7]={0};
    const FactoryRailGeometry top_geometry[7]={
        FACTORY_RAIL_CURVE_SE,FACTORY_RAIL_HORIZONTAL,
        FACTORY_RAIL_HORIZONTAL,FACTORY_RAIL_HORIZONTAL,
        FACTORY_RAIL_HORIZONTAL,FACTORY_RAIL_HORIZONTAL,
        FACTORY_RAIL_CURVE_SW};
    const FactoryRailGeometry bottom_geometry[7]={
        FACTORY_RAIL_CURVE_NE,FACTORY_RAIL_HORIZONTAL,
        FACTORY_RAIL_HORIZONTAL,FACTORY_RAIL_HORIZONTAL,
        FACTORY_RAIL_HORIZONTAL,FACTORY_RAIL_HORIZONTAL,
        FACTORY_RAIL_CURVE_NW};
    for(int32_t index=0;index<7;++index){
        top[index]=place_and_get(s,(FactoryCommand){FACTORY_COMMAND_PLACE_RAIL,
            {.place_rail={ox+index+2,oy+3,top_geometry[index]}}});
        (void)place_and_get(s,(FactoryCommand){FACTORY_COMMAND_PLACE_RAIL,
            {.place_rail={ox+index+2,oy+7,bottom_geometry[index]}}});
    }
    for(int32_t index=0;index<3;++index){
        (void)place_and_get(s,(FactoryCommand){FACTORY_COMMAND_PLACE_RAIL,
            {.place_rail={ox+2,oy+index+4,FACTORY_RAIL_VERTICAL}}});
        (void)place_and_get(s,(FactoryCommand){FACTORY_COMMAND_PLACE_RAIL,
            {.place_rail={ox+8,oy+index+4,FACTORY_RAIL_VERTICAL}}});
    }
    FactoryEntityId load_station=place_and_get(s,(FactoryCommand){
        FACTORY_COMMAND_PLACE_RAIL_STATION,
        {.place_rail_station={coal_x,coal_y+3,FACTORY_DIRECTION_SOUTH}}});
    FactoryEntityId unload_station=place_and_get(s,(FactoryCommand){
        FACTORY_COMMAND_PLACE_RAIL_STATION,
        {.place_rail_station={coal_x+2,coal_y+9,FACTORY_DIRECTION_NORTH}}});
    FactoryEntityId extractor=place_and_get(s,(FactoryCommand){
        FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor={coal_x,coal_y,FACTORY_DIRECTION_SOUTH}}});
    CHECK(factory_simulation_get_command_result(s,0U)->construction_depot_id
        ==depot);
    FactoryEntityId coal_belt=place_and_get(s,(FactoryCommand){
        FACTORY_COMMAND_PLACE_BELT,
        {.place_belt={coal_x,coal_y+1,FACTORY_DIRECTION_SOUTH}}});
    FactoryEntityId load_inserter=place_and_get(s,(FactoryCommand){
        FACTORY_COMMAND_PLACE_INSERTER,
        {.place_inserter={coal_x,coal_y+2,FACTORY_DIRECTION_SOUTH}}});
    (void)place_and_get(s,(FactoryCommand){FACTORY_COMMAND_PLACE_POWER_POLE,
        {.place_power_pole={coal_x-1,coal_y+2}}});
    s->fixture_initial_generator_fuel=FACTORY_TEST_GENERATOR_FUEL_QUANTITY;
    (void)place_and_get(s,(FactoryCommand){FACTORY_COMMAND_PLACE_POWER_GENERATOR,
        {.place_power_generator={coal_x-1,coal_y+1}}});
    FactoryEntityId unload_inserter=place_and_get(s,(FactoryCommand){
        FACTORY_COMMAND_PLACE_INSERTER,
        {.place_inserter={coal_x+2,coal_y+10,FACTORY_DIRECTION_SOUTH}}});
    FactoryEntityId refinery=place_and_get(s,(FactoryCommand){
        FACTORY_COMMAND_PLACE_REFINERY,
        {.place_refinery={coal_x+2,coal_y+11,FACTORY_DIRECTION_NORTH,
            FACTORY_DIRECTION_EAST}}});
    FactoryEntityId output_inserter=place_and_get(s,(FactoryCommand){
        FACTORY_COMMAND_PLACE_INSERTER,
        {.place_inserter={coal_x+3,coal_y+11,FACTORY_DIRECTION_EAST}}});
    FactoryEntityId steel_storage=place_and_get(s,(FactoryCommand){
        FACTORY_COMMAND_PLACE_STORAGE,
        {.place_storage={coal_x+4,coal_y+11}}});
    (void)place_and_get(s,(FactoryCommand){FACTORY_COMMAND_PLACE_POWER_POLE,
        {.place_power_pole={coal_x+2,coal_y+12}}});
    s->fixture_initial_generator_fuel=FACTORY_TEST_GENERATOR_FUEL_QUANTITY;
    (void)place_and_get(s,(FactoryCommand){FACTORY_COMMAND_PLACE_POWER_GENERATOR,
        {.place_power_generator={coal_x+1,coal_y+12}}});
    FactoryEntityId signal=place_and_get(s,(FactoryCommand){
        FACTORY_COMMAND_PLACE_RAIL_SIGNAL,
        {.place_rail_signal={ox+5,oy+4,FACTORY_DIRECTION_EAST}}});
    FactoryRailSignalInspection signal_state;
    CHECK(factory_simulation_get_rail_signal(s,signal,&signal_state)
        &&signal_state.connected);
    FactoryEntityId wagon=place_and_get(s,(FactoryCommand){
        FACTORY_COMMAND_PLACE_CARGO_WAGON,
        {.place_cargo_wagon={top[1],FACTORY_DIRECTION_EAST}}});
    FactoryEntityId train=place_and_get(s,(FactoryCommand){
        FACTORY_COMMAND_PLACE_LOCOMOTIVE,
        {.place_locomotive={top[2],FACTORY_DIRECTION_EAST}}});

    FactoryCommand setup[]={
        {FACTORY_COMMAND_COUPLE_REAR_WAGON,
            {.couple_rear_wagon={train,wagon}}},
        {FACTORY_COMMAND_SET_RAIL_STATION_FREIGHT_ITEM,
            {.set_rail_station_freight_item={load_station,FACTORY_ITEM_COAL}}},
        {FACTORY_COMMAND_SET_RAIL_STATION_FREIGHT_MODE,
            {.set_rail_station_freight_mode={load_station,
                FACTORY_RAIL_STATION_FREIGHT_LOAD}}},
        {FACTORY_COMMAND_SET_RAIL_STATION_FREIGHT_ITEM,
            {.set_rail_station_freight_item={unload_station,FACTORY_ITEM_COAL}}},
        {FACTORY_COMMAND_SET_RAIL_STATION_FREIGHT_MODE,
            {.set_rail_station_freight_mode={unload_station,
                FACTORY_RAIL_STATION_FREIGHT_UNLOAD}}},
        {FACTORY_COMMAND_SET_REFINERY_RECIPE,
            {.set_refinery_recipe={refinery,FACTORY_RECIPE_STEEL}}},
        {FACTORY_COMMAND_TRAIN_SCHEDULE_ADD_STOP,
            {.train_schedule_add_stop={train,load_station,
                FACTORY_TRAIN_WAIT_CARGO_FULL,0U}}},
        {FACTORY_COMMAND_TRAIN_SCHEDULE_ADD_STOP,
            {.train_schedule_add_stop={train,unload_station,
                FACTORY_TRAIN_WAIT_CARGO_EMPTY,0U}}},
        {FACTORY_COMMAND_TRAIN_SCHEDULE_SET_ENABLED,
            {.train_schedule_set_enabled={train,true}}}
    };
    for(size_t i=0U;i<sizeof(setup)/sizeof(setup[0]);++i)submit(s,setup[i]);
    ticks(s,1U);
    for(size_t i=0U;i<sizeof(setup)/sizeof(setup[0]);++i)
        CHECK(factory_simulation_get_command_result(s,i)->result
            ==FACTORY_RESULT_OK);

    FactoryTelemetryConfig telemetry_config={32U,1200U,128U};
    FactoryTelemetry *telemetry=factory_telemetry_create(&telemetry_config);
    FactorySimulation *loaded=NULL;
    bool saw_partial_load=false,saw_loaded_travel=false,saw_unload=false;
    uint32_t steel_cycles=0U;
    for(uint32_t iteration=0U;iteration<5200U&&steel_cycles<3U;++iteration){
        FactoryRefinery current;
        CHECK(factory_simulation_get_refinery(s,refinery,&current));
        if(!current.processing&&current.output_item==FACTORY_ITEM_NONE
            &&current.input_amount==0U){
            CHECK(factory_logistics_endpoint_insert(s,
                (FactoryLogisticsEndpoint){refinery,
                    FACTORY_LOGISTICS_SLOT_INPUT},FACTORY_ITEM_IRON_PLATE)
                ==FACTORY_LOGISTICS_RESULT_OK);
            CHECK(factory_logistics_endpoint_insert(s,
                (FactoryLogisticsEndpoint){refinery,
                    FACTORY_LOGISTICS_SLOT_INPUT},FACTORY_ITEM_IRON_PLATE)
                ==FACTORY_LOGISTICS_RESULT_OK);
            if(loaded!=NULL){
                CHECK(factory_logistics_endpoint_insert(loaded,
                    (FactoryLogisticsEndpoint){refinery,
                        FACTORY_LOGISTICS_SLOT_INPUT},
                    FACTORY_ITEM_IRON_PLATE)==FACTORY_LOGISTICS_RESULT_OK);
                CHECK(factory_logistics_endpoint_insert(loaded,
                    (FactoryLogisticsEndpoint){refinery,
                        FACTORY_LOGISTICS_SLOT_INPUT},
                    FACTORY_ITEM_IRON_PLATE)==FACTORY_LOGISTICS_RESULT_OK);
            }
        }
        CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
        CHECK(factory_telemetry_observe_step(telemetry,s)
            ==FACTORY_TELEMETRY_RESULT_OK);
        if(loaded!=NULL){
            CHECK(factory_simulation_tick(loaded)==FACTORY_RESULT_OK);
            FactoryCargoWagonInspection loaded_wagon;
            FactoryStorage loaded_storage;
            CHECK(factory_simulation_get_cargo_wagon(loaded,wagon,
                &loaded_wagon));
            CHECK(factory_simulation_get_storage(loaded,steel_storage,
                &loaded_storage));
            CHECK(factory_world_get_tile(factory_simulation_get_world(s),
                    coal_x,coal_y)->resource_amount
                ==factory_world_get_tile(factory_simulation_get_world(loaded),
                    coal_x,coal_y)->resource_amount);
            CHECK(factory_simulation_get_tick(s)
                ==factory_simulation_get_tick(loaded));
        }
        FactoryCargoWagonInspection wagon_state;
        FactoryLocomotiveInspection train_state;
        FactoryRailStationInspection unload_state;
        FactoryStorage steel_state;
        CHECK(factory_simulation_get_cargo_wagon(s,wagon,&wagon_state));
        CHECK(factory_simulation_get_locomotive(s,train,&train_state));
        CHECK(factory_simulation_get_rail_station(s,unload_station,&unload_state));
        CHECK(factory_simulation_get_storage(s,steel_storage,&steel_state));
        saw_partial_load|=wagon_state.cargo_quantity>0U
            &&wagon_state.cargo_quantity<FACTORY_CARGO_WAGON_CAPACITY;
        saw_loaded_travel|=wagon_state.cargo_quantity
                ==FACTORY_CARGO_WAGON_CAPACITY
            &&train_state.route_status==FACTORY_TRAIN_ROUTE_ACTIVE;
        saw_unload|=unload_state.freight_quantity>0U;
        steel_cycles=steel_state.steel_amount;
        CHECK(coal_and_steel_accounting(s,coal_x,coal_y,extractor,
            coal_belt,load_inserter,load_station,wagon,unload_station,
            unload_inserter,refinery,output_inserter,steel_storage)
            ==initial_coal);
        if(loaded==NULL&&saw_loaded_travel){
            FactorySnapshotBuffer checkpoint={0};
            CHECK(factory_simulation_create_snapshot(s,&checkpoint)
                ==FACTORY_RESULT_OK);
            CHECK(factory_simulation_load_snapshot(checkpoint.data,
                checkpoint.size,&loaded)==FACTORY_RESULT_OK);
            factory_snapshot_buffer_destroy(&checkpoint);
        }
    }
    CHECK(saw_partial_load&&saw_loaded_travel&&saw_unload);
    CHECK(loaded!=NULL&&steel_cycles>=3U);
    CHECK(loaded!=NULL&&snapshots_equal(s,loaded));
    if(!(saw_partial_load&&saw_loaded_travel&&saw_unload
        &&loaded!=NULL&&steel_cycles>=3U)){
        FactoryExtractor debug_extractor;
        FactoryCargoWagonInspection debug_wagon;
        FactoryRailStationInspection debug_load,debug_unload;
        FactoryLocomotiveInspection debug_train;
        FactoryRefinery debug_refinery;
        FactoryInserter debug_inserter;
        factory_simulation_get_extractor(s,extractor,&debug_extractor);
        factory_simulation_get_cargo_wagon(s,wagon,&debug_wagon);
        factory_simulation_get_rail_station(s,load_station,&debug_load);
        factory_simulation_get_rail_station(s,unload_station,&debug_unload);
        factory_simulation_get_locomotive(s,train,&debug_train);
        factory_simulation_get_refinery(s,refinery,&debug_refinery);
        factory_simulation_get_inserter(s,load_inserter,&debug_inserter);
        fprintf(stderr,"freight debug extractor=%u progress=%u inserter=%u/%u/%u wagon=%u load=%u unload=%u route=%u stop=%u refinery coal=%u processing=%u steel=%u\n",
            debug_extractor.output_amount,debug_extractor.production_progress,
            debug_inserter.held_item,debug_inserter.held_amount,
            debug_inserter.progress,
            debug_wagon.cargo_quantity,debug_load.freight_quantity,
            debug_unload.freight_quantity,(unsigned)debug_train.route_status,
            debug_train.current_stop_index,
            debug_refinery.secondary_input_amount,
            debug_refinery.processing?1U:0U,steel_cycles);
    }
    CHECK(factory_simulation_get_locomotive(s,train,
        &(FactoryLocomotiveInspection){0}));
    {
        FactoryTelemetryItemMetrics coal_metrics,steel_metrics;
        CHECK(factory_telemetry_get_item_metrics(telemetry,FACTORY_ITEM_COAL,
            FACTORY_TELEMETRY_WINDOW_LONG,&coal_metrics));
        CHECK(factory_telemetry_get_item_metrics(telemetry,FACTORY_ITEM_STEEL,
            FACTORY_TELEMETRY_WINDOW_LONG,&steel_metrics));
        CHECK(coal_metrics.extracted_quantity>0U);
        CHECK(coal_metrics.transferred_quantity>=100U);
        CHECK(steel_metrics.produced_quantity>=3U);
    }
    factory_simulation_destroy(loaded);
    factory_telemetry_destroy(telemetry);
    factory_simulation_destroy(s);factory_world_destroy(world);
}

static void test_new_state_preflight_rollback(void)
{
    FactoryWorld *world=factory_world_create(8U,6U);
    FactorySimulation *s=factory_simulation_create_with_construction_units(
        world,UINT32_MAX);
    FactorySnapshotBuffer before={0},after={0};
    FactoryRefinery refinery;
    FactoryResearchLabInspection lab;
    s->research.completed_bits|=
        (UINT64_C(1)<<FACTORY_TECHNOLOGY_BASIC_AUTOMATION)
        |(UINT64_C(1)<<FACTORY_TECHNOLOGY_FLUID_HANDLING);
    s->research.progress[0].completed_units=2U;
    s->research.progress[1].completed_units=2U;
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_REFINERY,
        {.place_refinery={1,1,FACTORY_DIRECTION_WEST,
            FACTORY_DIRECTION_EAST}}});
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_RESEARCH_LAB,
        {.place_research_lab={4,1}}});
    CHECK(factory_test_submit_power_pair(s,2,3,3,3));
    ticks(s,1U);
    submit(s,(FactoryCommand){FACTORY_COMMAND_SET_REFINERY_RECIPE,
        {.set_refinery_recipe={1U,FACTORY_RECIPE_STEEL}}});
    ticks(s,1U);
    for(uint32_t i=0U;i<2U;++i)CHECK(factory_logistics_endpoint_insert(s,
        (FactoryLogisticsEndpoint){1U,FACTORY_LOGISTICS_SLOT_INPUT},
        FACTORY_ITEM_IRON_PLATE)==FACTORY_LOGISTICS_RESULT_OK);
    CHECK(factory_logistics_endpoint_insert(s,
        (FactoryLogisticsEndpoint){1U,FACTORY_LOGISTICS_SLOT_INPUT},
        FACTORY_ITEM_COAL)==FACTORY_LOGISTICS_RESULT_OK);
    CHECK(factory_logistics_endpoint_insert(s,
        (FactoryLogisticsEndpoint){2U,
            FACTORY_LOGISTICS_SLOT_RESEARCH_LAB_INPUT},
        FACTORY_ITEM_ADVANCED_SCIENCE)==FACTORY_LOGISTICS_RESULT_OK);
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_STORAGE,
        {.place_storage={6,1}}});
    CHECK(factory_simulation_create_snapshot(s,&before)==FACTORY_RESULT_OK);
    const uint64_t tick_before=factory_simulation_get_tick(s);
    const size_t events_before=factory_simulation_get_event_count(s);
    factory_tick_preflight_test_fail_allocations_after(0U);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OUT_OF_MEMORY);
    factory_tick_preflight_test_fail_allocations_after(SIZE_MAX);
    CHECK(factory_simulation_get_tick(s)==tick_before);
    CHECK(factory_simulation_get_pending_command_count(s)==1U);
    CHECK(factory_simulation_get_event_count(s)==events_before);
    CHECK(factory_simulation_get_refinery(s,1U,&refinery)
        &&refinery.input_amount==2U
        &&refinery.secondary_input_item==FACTORY_ITEM_COAL
        &&refinery.secondary_input_amount==1U&&!refinery.processing);
    CHECK(factory_simulation_get_research_lab(s,2U,&lab)==FACTORY_RESULT_OK
        &&lab.science_item==FACTORY_ITEM_ADVANCED_SCIENCE
        &&lab.science_quantity==1U);
    CHECK(factory_simulation_create_snapshot(s,&after)==FACTORY_RESULT_OK);
    CHECK(before.size==after.size
        &&memcmp(before.data,after.data,before.size)==0);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(s,0U)->result
        ==FACTORY_RESULT_OK);
    factory_snapshot_buffer_destroy(&before);
    factory_snapshot_buffer_destroy(&after);
    factory_simulation_destroy(s);factory_world_destroy(world);
}

int main(void)
{
    test_catalog_and_physical_chain();
    test_finite_coal_depletion();
    test_generated_coal_autonomous_freight_to_steel();
    test_new_state_preflight_rollback();
    if(failures!=0)return 1;
    puts("All advanced science tests passed.");
    return 0;
}
