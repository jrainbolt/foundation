#include "foundation/content.h"
#include "foundation/construction_depot.h"
#include "foundation/simulation.h"
#include "logistics_endpoint_internal.h"
#include "simulation_internal.h"

#include <stdbool.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static int failures;
#define CHECK(c) do {if(!(c)){fprintf(stderr,"FAIL %s:%d: %s\n", \
    __FILE__,__LINE__,#c);++failures;}}while(false)

static void submit(FactorySimulation *simulation,FactoryCommand command)
{CHECK(factory_simulation_submit_command(simulation,&command)==FACTORY_RESULT_OK);}

static void tick(FactorySimulation *simulation)
{CHECK(factory_simulation_tick(simulation)==FACTORY_RESULT_OK);}

static void insert(FactorySimulation *simulation,FactoryEntityId entity,
    FactoryLogisticsSlot slot,FactoryItemType item)
{CHECK(factory_logistics_endpoint_insert(simulation,(FactoryLogisticsEndpoint){
    entity,slot},item)==FACTORY_LOGISTICS_RESULT_OK);}

static void transfer(FactorySimulation *simulation,FactoryEntityId source,
    FactoryEntityId destination,FactoryLogisticsSlot destination_slot,
    FactoryItemType item)
{CHECK(factory_logistics_endpoint_transfer(simulation,
    (FactoryLogisticsEndpoint){source,FACTORY_LOGISTICS_SLOT_OUTPUT},
    (FactoryLogisticsEndpoint){destination,destination_slot},item)
    ==FACTORY_LOGISTICS_RESULT_OK);}

static void test_physical_basic_science_research(void)
{
    FactoryWorld *world=factory_world_create(12U,6U);
    FactorySimulation *simulation=
        factory_simulation_create_with_construction_units(world,UINT32_MAX);
    FactoryEntityId gear=1U,electronic=2U,science=3U,lab=4U;
    const FactoryAssemblerRecipe *definition=
        factory_content_assembler_recipe_get(
            FACTORY_ASSEMBLER_RECIPE_BASIC_SCIENCE);
    CHECK(definition!=NULL&&definition->required_unlock==FACTORY_UNLOCK_NONE
        &&definition->input_items[0]==FACTORY_ITEM_IRON_GEAR
        &&definition->input_items[1]==FACTORY_ITEM_ELECTRONIC_COMPONENT
        &&definition->output_item==FACTORY_ITEM_BASIC_SCIENCE);
    submit(simulation,(FactoryCommand){FACTORY_COMMAND_PLACE_ASSEMBLER,
        {.place_assembler={1,1,FACTORY_DIRECTION_EAST}}});
    submit(simulation,(FactoryCommand){FACTORY_COMMAND_PLACE_ASSEMBLER,
        {.place_assembler={3,1,FACTORY_DIRECTION_EAST}}});
    submit(simulation,(FactoryCommand){FACTORY_COMMAND_PLACE_ASSEMBLER,
        {.place_assembler={5,1,FACTORY_DIRECTION_EAST}}});
    submit(simulation,(FactoryCommand){FACTORY_COMMAND_PLACE_RESEARCH_LAB,
        {.place_research_lab={7,1}}});
    submit(simulation,(FactoryCommand){FACTORY_COMMAND_PLACE_POWER_POLE,
        {.place_power_pole={3,3}}});
    submit(simulation,(FactoryCommand){FACTORY_COMMAND_PLACE_POWER_POLE,
        {.place_power_pole={7,3}}});
    submit(simulation,(FactoryCommand){FACTORY_COMMAND_PLACE_SOLAR_GENERATOR,
        {.place_solar_generator={5,3}}});
    submit(simulation,(FactoryCommand){FACTORY_COMMAND_PLACE_SOLAR_GENERATOR,
        {.place_solar_generator={9,3}}});
    tick(simulation);
    submit(simulation,(FactoryCommand){FACTORY_COMMAND_SET_ASSEMBLER_RECIPE,
        {.set_assembler_recipe={gear,FACTORY_ASSEMBLER_RECIPE_IRON_GEAR}}});
    submit(simulation,(FactoryCommand){FACTORY_COMMAND_SET_ASSEMBLER_RECIPE,
        {.set_assembler_recipe={electronic,
            FACTORY_ASSEMBLER_RECIPE_ELECTRONIC_COMPONENT}}});
    submit(simulation,(FactoryCommand){FACTORY_COMMAND_SET_ASSEMBLER_RECIPE,
        {.set_assembler_recipe={science,
            FACTORY_ASSEMBLER_RECIPE_BASIC_SCIENCE}}});
    tick(simulation);
    while(factory_simulation_clock_get_time_of_day(simulation)<900U)
        tick(simulation);
    uint64_t first_science_tick=0U;
    for(uint32_t cycle=0U;cycle<4U;++cycle){
        insert(simulation,gear,FACTORY_LOGISTICS_SLOT_ASSEMBLER_INPUT_0,
            FACTORY_ITEM_IRON_PLATE);
        insert(simulation,gear,FACTORY_LOGISTICS_SLOT_ASSEMBLER_INPUT_0,
            FACTORY_ITEM_IRON_PLATE);
        insert(simulation,electronic,FACTORY_LOGISTICS_SLOT_ASSEMBLER_INPUT_0,
            FACTORY_ITEM_IRON_PLATE);
        insert(simulation,electronic,FACTORY_LOGISTICS_SLOT_ASSEMBLER_INPUT_1,
            FACTORY_ITEM_COPPER_PLATE);
        for(uint32_t i=0U;i<15U;++i)tick(simulation);
        transfer(simulation,gear,science,
            FACTORY_LOGISTICS_SLOT_ASSEMBLER_INPUT_0,FACTORY_ITEM_IRON_GEAR);
        transfer(simulation,electronic,science,
            FACTORY_LOGISTICS_SLOT_ASSEMBLER_INPUT_1,
            FACTORY_ITEM_ELECTRONIC_COMPONENT);
        for(uint32_t i=0U;i<15U;++i)tick(simulation);
        if(first_science_tick==0U)first_science_tick=
            factory_simulation_get_tick(simulation);
        transfer(simulation,science,lab,
            FACTORY_LOGISTICS_SLOT_RESEARCH_LAB_INPUT,
            FACTORY_ITEM_BASIC_SCIENCE);
    }
    FactoryResearchLabInspection inspection={0};
    CHECK(factory_simulation_get_research_lab(simulation,lab,&inspection)
        ==FACTORY_RESULT_OK&&inspection.science_item==FACTORY_ITEM_BASIC_SCIENCE
        &&inspection.science_quantity==4U);
    submit(simulation,(FactoryCommand){FACTORY_COMMAND_SELECT_RESEARCH,
        {.select_research={FACTORY_TECHNOLOGY_BASIC_AUTOMATION}}});
    for(uint32_t i=0U;i<20U
        &&!factory_simulation_is_technology_completed(simulation,
            FACTORY_TECHNOLOGY_BASIC_AUTOMATION);++i)tick(simulation);
    CHECK(factory_simulation_is_technology_completed(simulation,
        FACTORY_TECHNOLOGY_BASIC_AUTOMATION));
    CHECK(factory_simulation_has_unlock(simulation,FACTORY_UNLOCK_AUTOMATION));
    CHECK(factory_simulation_get_research_lab(simulation,lab,&inspection)
        ==FACTORY_RESULT_OK&&inspection.science_quantity==0U);
    printf("physical Basic Science first tick=%llu Basic Automation tick=%llu\n",
        (unsigned long long)first_science_tick,
        (unsigned long long)factory_simulation_get_tick(simulation));
    factory_simulation_destroy(simulation);factory_world_destroy(world);
}

typedef struct {
    int32_t x;
    int32_t y;
    uint32_t quantity;
    uint32_t distance;
} ResourceCandidate;

static uint32_t coordinate_distance(int32_t ax,int32_t ay,int32_t bx,int32_t by)
{
    return (uint32_t)(abs(ax-bx)+abs(ay-by));
}

static ResourceCandidate closest_resource(const FactoryWorld *world,
    FactoryResourceType resource,uint32_t minimum_distance)
{
    ResourceCandidate result={0,0,0U,UINT_MAX};
    int32_t start_x=factory_world_get_start_x(world);
    int32_t start_y=factory_world_get_start_y(world);
    for(int32_t y=0;y<48;++y)for(int32_t x=0;x<64;++x){
        const FactoryTile *tile=factory_world_get_tile(world,x,y);
        uint32_t distance=coordinate_distance(start_x,start_y,x,y);
        if(tile!=NULL&&tile->resource==resource&&distance>=minimum_distance
            &&distance<result.distance)
            result=(ResourceCandidate){x,y,tile->resource_amount,distance};
    }
    return result;
}

static uint32_t required_single_depot_radius(int32_t start_x,int32_t start_y,
    ResourceCandidate iron,ResourceCandidate copper,ResourceCandidate coal)
{
    int32_t x[4]={start_x,iron.x,copper.x,coal.x};
    int32_t y[4]={start_y,iron.y,copper.y,coal.y};
    int32_t minimum_sum=INT_MAX,maximum_sum=INT_MIN;
    int32_t minimum_difference=INT_MAX,maximum_difference=INT_MIN;
    for(size_t i=0U;i<4U;++i){
        int32_t sum=x[i]+y[i],difference=x[i]-y[i];
        if(sum<minimum_sum)minimum_sum=sum;
        if(sum>maximum_sum)maximum_sum=sum;
        if(difference<minimum_difference)minimum_difference=difference;
        if(difference>maximum_difference)maximum_difference=difference;
    }
    int32_t span=maximum_sum-minimum_sum;
    if(maximum_difference-minimum_difference>span)
        span=maximum_difference-minimum_difference;
    return (uint32_t)(span+1)/2U;
}

static void test_candidate_seed_coverage(void)
{
    for(uint64_t seed=1U;seed<=20U;++seed){
        FactoryWorldGenerationConfig generation;
        FactoryWorld *world=factory_world_create_with_seed(64U,48U,seed);
        factory_world_generation_default_config(&generation);
        generation.water_threshold=0U;
        generation.rock_threshold=0U;
        CHECK(factory_world_generate(world,&generation)==FACTORY_RESULT_OK);
        ResourceCandidate iron=closest_resource(world,FACTORY_RESOURCE_IRON,0U);
        ResourceCandidate copper=closest_resource(world,FACTORY_RESOURCE_COPPER,0U);
        ResourceCandidate coal=closest_resource(world,FACTORY_RESOURCE_COAL,12U);
        uint32_t radius=required_single_depot_radius(
            factory_world_get_start_x(world),factory_world_get_start_y(world),
            iron,copper,coal);
        CHECK(radius>FACTORY_CONSTRUCTION_DEPOT_RADIUS);
        printf("seed %llu minimum start/resource coverage radius=%u\n",
            (unsigned long long)seed,radius);
        factory_world_destroy(world);
    }
}

static void test_remote_construction_supply_relay(void)
{
    FactoryWorldGenerationConfig generation;
    FactoryWorld *world=factory_world_create_with_seed(64U,48U,UINT64_C(6));
    FactorySimulation *simulation;
    FactoryConstructionDepot first={0},second={0};
    FactoryEntityId coverage_id=0U;
    bool complete_supply=true;
    factory_world_generation_default_config(&generation);
    generation.water_threshold=0U;
    generation.rock_threshold=0U;
    CHECK(factory_world_generate(world,&generation)==FACTORY_RESULT_OK);
    const int32_t start_x=factory_world_get_start_x(world);
    const int32_t start_y=factory_world_get_start_y(world);
    ResourceCandidate iron=closest_resource(world,FACTORY_RESOURCE_IRON,0U);
    ResourceCandidate copper=closest_resource(world,FACTORY_RESOURCE_COPPER,0U);
    ResourceCandidate coal=closest_resource(world,FACTORY_RESOURCE_COAL,12U);
    CHECK(iron.distance!=UINT_MAX&&copper.distance!=UINT_MAX
        &&coal.distance!=UINT_MAX);
    simulation=factory_simulation_create_with_construction_units(world,
        FACTORY_CONSTRUCTION_COST_CONSTRUCTION_DEPOT+500U);
    submit(simulation,(FactoryCommand){FACTORY_COMMAND_PLACE_CONSTRUCTION_DEPOT,
        {.place_construction_depot={start_x,start_y}}});
    tick(simulation);
    const FactoryCommandResult *result=
        factory_simulation_get_command_result(simulation,0U);
    CHECK(result!=NULL&&result->result==FACTORY_RESULT_OK
        &&factory_simulation_construction_bootstrap_completed(simulation));
    FactoryEntityId first_id=result!=NULL?result->entity_id:0U;
    CHECK(factory_simulation_construction_units(simulation)==0U
        &&factory_simulation_get_construction_depot(simulation,first_id,&first)
        &&first.material_quantity==500U);

    /* This relay tile is exactly radius eight from the starter Depot and
     * within radius eight of seed 6's selected Coal tile. */
    FactoryCommand supply_chain[]={
        {FACTORY_COMMAND_PLACE_INSERTER,
            {.place_inserter={33,24,FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_BELT,
            {.place_belt={34,24,FACTORY_DIRECTION_NORTH}}},
        {FACTORY_COMMAND_PLACE_BELT,
            {.place_belt={34,23,FACTORY_DIRECTION_NORTH}}},
        {FACTORY_COMMAND_PLACE_BELT,
            {.place_belt={34,22,FACTORY_DIRECTION_NORTH}}},
        {FACTORY_COMMAND_PLACE_BELT,
            {.place_belt={34,21,FACTORY_DIRECTION_NORTH}}},
        {FACTORY_COMMAND_PLACE_BELT,
            {.place_belt={34,20,FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_INSERTER,
            {.place_inserter={35,20,FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_POWER_POLE,
            {.place_power_pole={33,22}}},
        {FACTORY_COMMAND_PLACE_POWER_POLE,
            {.place_power_pole={35,22}}},
        {FACTORY_COMMAND_PLACE_POWER_GENERATOR,
            {.place_power_generator={32,22}}},
        {FACTORY_COMMAND_PLACE_CONSTRUCTION_DEPOT,
            {.place_construction_depot={36,20}}}
    };
    for(size_t i=0U;i<sizeof(supply_chain)/sizeof(supply_chain[0]);++i)
        submit(simulation,supply_chain[i]);
    tick(simulation);
    for(size_t i=0U;i<sizeof(supply_chain)/sizeof(supply_chain[0]);++i)
        CHECK(factory_simulation_get_command_result(simulation,i)->result
            ==FACTORY_RESULT_OK);
    FactoryEntityId second_id=factory_simulation_get_command_result(
        simulation,sizeof(supply_chain)/sizeof(supply_chain[0])-1U)->entity_id;
    CHECK(factory_simulation_get_construction_depot(simulation,first_id,&first)
        &&first.material_quantity==426U
        &&factory_simulation_get_construction_depot(simulation,second_id,&second)
        &&second.material_quantity==0U);
    CHECK(factory_simulation_position_has_construction_coverage(simulation,
        coal.x,coal.y,&coverage_id,&complete_supply)
        &&coverage_id==second_id&&!complete_supply);
    for(uint32_t fuel=0U;fuel<5U;++fuel)
        CHECK(factory_logistics_endpoint_insert(simulation,
            (FactoryLogisticsEndpoint){11U,FACTORY_LOGISTICS_SLOT_BURNER_INPUT},
            FACTORY_ITEM_BIOMASS_PELLET)==FACTORY_LOGISTICS_RESULT_OK);
    for(uint32_t ticks=0U;ticks<400U&&second.material_quantity<10U;++ticks){
        tick(simulation);
        CHECK(factory_simulation_get_construction_depot(
            simulation,second_id,&second));
    }
    CHECK(second.material_quantity==10U);
    submit(simulation,(FactoryCommand){FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor={coal.x,coal.y,FACTORY_DIRECTION_SOUTH}}});
    tick(simulation);
    result=factory_simulation_get_command_result(simulation,0U);
    CHECK(result!=NULL&&result->result==FACTORY_RESULT_OK
        &&result->construction_depot_id==second_id);
    CHECK(factory_simulation_get_construction_depot(simulation,first_id,&first)
        &&first.material_quantity<=416U
        &&factory_simulation_get_construction_depot(simulation,second_id,&second)
        &&second.material_quantity==0U);
    printf("seed 6 coverage relay: start=(%d,%d) iron=(%d,%d d=%u q=%u) "
        "copper=(%d,%d d=%u q=%u) coal=(%d,%d d=%u q=%u); "
        "relay supplied=%u through 2 Inserters/5 Belts and funded remote extractor\n",
        start_x,start_y,iron.x,iron.y,iron.distance,iron.quantity,
        copper.x,copper.y,copper.distance,copper.quantity,
        coal.x,coal.y,coal.distance,coal.quantity,
        FACTORY_CONSTRUCTION_COST_EXTRACTOR);
    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

int main(void)
{
    test_physical_basic_science_research();
    test_candidate_seed_coverage();
    test_remote_construction_supply_relay();
    return failures==0?0:1;
}
