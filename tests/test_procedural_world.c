#include <foundation/content.h>
#include <foundation/presentation.h>
#include <foundation/snapshot.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#c);++failures;}}while(false)

static void test_seed_terrain_and_content(void)
{
    FactoryWorld *a=factory_world_create(4U,3U);
    FactoryWorld *b=factory_world_create_with_seed(4U,3U,UINT64_C(99));
    CHECK(a!=NULL&&b!=NULL);
    CHECK(factory_world_get_seed(a)==FACTORY_WORLD_DEFAULT_SEED);
    CHECK(factory_world_get_seed(b)==UINT64_C(99));
    CHECK(factory_content_terrain_definition_count()==3U);
    CHECK(factory_content_validate());
    {
        FactoryTerrainDefinition duplicate[3]={
            {FACTORY_TERRAIN_GROUND,true,0U},
            {FACTORY_TERRAIN_GROUND,false,0U},
            {FACTORY_TERRAIN_ROCK,false,0U}};
        FactoryTerrainDefinition bad[3]={
            {FACTORY_TERRAIN_GROUND,true,UINT32_MAX},
            {FACTORY_TERRAIN_WATER,false,0U},
            {FACTORY_TERRAIN_ROCK,false,0U}};
        CHECK(!factory_content_terrain_definitions_validate(duplicate,3U));
        CHECK(!factory_content_terrain_definitions_validate(bad,3U));
    }
    CHECK(factory_content_terrain_definition_get(FACTORY_TERRAIN_GROUND)->buildable);
    CHECK(!factory_content_terrain_definition_get(FACTORY_TERRAIN_WATER)->buildable);
    CHECK(factory_world_initialize_terrain(b,1,1,FACTORY_TERRAIN_WATER)==FACTORY_RESULT_OK);
    CHECK(factory_world_initialize_terrain(b,2,1,FACTORY_TERRAIN_ROCK)==FACTORY_RESULT_OK);
    CHECK(factory_world_get_terrain(b,1,1)==FACTORY_TERRAIN_WATER);
    CHECK(factory_world_get_terrain(b,-1,0)==FACTORY_TERRAIN_NONE);
    CHECK(factory_world_add_resource(b,1,1,FACTORY_RESOURCE_IRON,10U)==FACTORY_RESULT_INVALID_ARGUMENT);
    CHECK(factory_world_add_resource(b,0,0,FACTORY_RESOURCE_IRON,10U)==FACTORY_RESULT_OK);
    CHECK(factory_world_validate(a));CHECK(factory_world_validate(b));
    factory_world_destroy(a);factory_world_destroy(b);
}

static void test_transactional_construction(void)
{
    FactoryWorld *world=factory_world_create(3U,2U);
    FactorySimulation *simulation;
    FactoryCommand command={0};
    const FactoryCommandResult *result;
    CHECK(factory_world_initialize_terrain(world,1,0,FACTORY_TERRAIN_WATER)==FACTORY_RESULT_OK);
    simulation=factory_simulation_create_with_construction_units(world,100U);
    CHECK(factory_world_initialize_terrain(world,2,0,FACTORY_TERRAIN_ROCK)==FACTORY_RESULT_INVALID_STATE);
    command.type=FACTORY_COMMAND_PLACE_STORAGE;
    command.data.place_storage.x=1;command.data.place_storage.y=0;
    CHECK(factory_simulation_submit_command(simulation,&command)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(simulation)==FACTORY_RESULT_OK);
    result=factory_simulation_get_command_result(simulation,0U);
    CHECK(result!=NULL&&result->result==FACTORY_RESULT_TERRAIN_BLOCKED);
    CHECK(factory_simulation_construction_units(simulation)==100U);
    CHECK(factory_simulation_get_event_count(simulation)==0U);
    command.data.place_storage.x=0;
    CHECK(factory_simulation_submit_command(simulation,&command)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(simulation)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(simulation,0U)->result==FACTORY_RESULT_OK);
    factory_simulation_destroy(simulation);factory_world_destroy(world);
}

static void test_snapshot_and_presentation(void)
{
    FactoryWorld *world=factory_world_create_with_seed(3U,2U,UINT64_C(1234));
    FactorySimulation *simulation,*loaded=NULL;
    FactorySnapshotBuffer bytes={0};
    FactoryPresentationSnapshot *presentation=factory_presentation_snapshot_create();
    CHECK(factory_world_initialize_terrain(world,2,0,FACTORY_TERRAIN_WATER)==FACTORY_RESULT_OK);
    CHECK(factory_world_initialize_terrain(world,0,1,FACTORY_TERRAIN_ROCK)==FACTORY_RESULT_OK);
    simulation=factory_simulation_create(world);
    CHECK(factory_presentation_snapshot_rebuild(presentation,simulation)==FACTORY_RESULT_OK);
    CHECK(factory_presentation_snapshot_get_terrain_count(presentation)==6U);
    CHECK(factory_presentation_snapshot_get_terrain(presentation,2U)->terrain_type==FACTORY_TERRAIN_WATER);
    CHECK(factory_presentation_snapshot_get_terrain(presentation,3U)->terrain_type==FACTORY_TERRAIN_ROCK);
    CHECK(factory_presentation_snapshot_get_terrain(presentation,6U)==NULL);
    CHECK(factory_simulation_create_snapshot(simulation,&bytes)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_load_snapshot(bytes.data,bytes.size,&loaded)==FACTORY_RESULT_OK);
    CHECK(factory_world_get_seed(factory_simulation_get_world(loaded))==UINT64_C(1234));
    CHECK(factory_world_get_terrain(factory_simulation_get_world(loaded),2,0)==FACTORY_TERRAIN_WATER);
    CHECK(factory_world_get_terrain(factory_simulation_get_world(loaded),0,1)==FACTORY_TERRAIN_ROCK);
    factory_presentation_snapshot_destroy(presentation);
    factory_snapshot_buffer_destroy(&bytes);
    factory_simulation_destroy(loaded);
    factory_simulation_destroy(simulation);factory_world_destroy(world);
}

static void test_generation(void)
{
    FactoryWorldGenerationConfig config;
    FactoryWorld *a=factory_world_create_with_seed(64U,48U,UINT64_C(42));
    FactoryWorld *b=factory_world_create_with_seed(64U,48U,UINT64_C(42));
    FactoryWorld *different=factory_world_create_with_seed(64U,48U,UINT64_C(43));
    uint32_t terrain_counts[4]={0},iron=0U,copper=0U,remote=0U;
    factory_world_generation_default_config(&config);
    CHECK(factory_world_generate(a,&config)==FACTORY_RESULT_OK);
    CHECK(factory_world_generate(b,&config)==FACTORY_RESULT_OK);
    CHECK(factory_world_generate(different,&config)==FACTORY_RESULT_OK);
    CHECK(factory_world_generation_checksum(a)==factory_world_generation_checksum(b));
    CHECK(factory_world_generation_checksum(a)!=factory_world_generation_checksum(different));
    CHECK(factory_world_generation_checksum(a)==UINT64_C(17182821196558947513));
    CHECK(factory_world_get_start_x(a)==32&&factory_world_get_start_y(a)==24);
    for(uint32_t y=0U;y<48U;++y)for(uint32_t x=0U;x<64U;++x){
        const FactoryTile *ta=factory_world_get_tile(a,(int32_t)x,(int32_t)y);
        const FactoryTile *tb=factory_world_get_tile(b,(int32_t)x,(int32_t)y);
        CHECK(ta->terrain==tb->terrain&&ta->resource==tb->resource
            &&ta->resource_amount==tb->resource_amount);
        ++terrain_counts[ta->terrain];
        if(ta->resource==FACTORY_RESOURCE_IRON)++iron;
        if(ta->resource==FACTORY_RESOURCE_COPPER)++copper;
        if(ta->resource!=FACTORY_RESOURCE_NONE
            &&(abs((int32_t)x-32)>11||abs((int32_t)y-24)>11)){
            ++remote;CHECK(ta->resource_amount>=900U);
        }
    }
    CHECK(terrain_counts[FACTORY_TERRAIN_GROUND]>terrain_counts[FACTORY_TERRAIN_WATER]);
    CHECK(terrain_counts[FACTORY_TERRAIN_GROUND]>terrain_counts[FACTORY_TERRAIN_ROCK]);
    CHECK(terrain_counts[FACTORY_TERRAIN_WATER]>0U&&terrain_counts[FACTORY_TERRAIN_ROCK]>0U);
    CHECK(iron>=3U&&copper>=3U);
    CHECK(remote>=3U);
    for(int32_t dy=-5;dy<=5;++dy)for(int32_t dx=-5;dx<=5;++dx)
        if(dx*dx+dy*dy<=25)CHECK(factory_world_get_terrain(a,32+dx,24+dy)==FACTORY_TERRAIN_GROUND);
    CHECK(factory_world_generate(a,&config)==FACTORY_RESULT_OK);
    {
        FactorySimulation *simulation=factory_simulation_create(a);
        FactorySimulation *simulation_b=factory_simulation_create(b);
        FactorySimulation *loaded=NULL;
        FactorySnapshotBuffer sa={0},sb={0};
        CHECK(factory_world_generate(a,&config)==FACTORY_RESULT_INVALID_STATE);
        CHECK(factory_simulation_create_snapshot(simulation,&sa)==FACTORY_RESULT_OK);
        CHECK(factory_simulation_create_snapshot(simulation_b,&sb)==FACTORY_RESULT_OK);
        CHECK(sa.size==sb.size&&memcmp(sa.data,sb.data,sa.size)==0);
        CHECK(factory_simulation_load_snapshot(sa.data,sa.size,&loaded)==FACTORY_RESULT_OK);
        CHECK(factory_world_generation_checksum(factory_simulation_get_world(loaded))
            ==factory_world_generation_checksum(a));
        factory_simulation_destroy(loaded);
        factory_snapshot_buffer_destroy(&sa);factory_snapshot_buffer_destroy(&sb);
        factory_simulation_destroy(simulation_b);
        factory_simulation_destroy(simulation);
    }
    factory_world_destroy(a);factory_world_destroy(b);factory_world_destroy(different);
}

static void test_generation_failure_is_transactional(void)
{
    FactoryWorldGenerationConfig config;
    FactoryWorld *world=factory_world_create_with_seed(8U,8U,77U);
    uint64_t before=factory_world_generation_checksum(world);
    factory_world_generation_default_config(&config);
    CHECK(factory_world_generate(world,&config)==FACTORY_RESULT_WORLD_GENERATION_FAILED);
    CHECK(factory_world_generation_checksum(world)==before);
    factory_world_destroy(world);
}

static void test_moderate_world(void)
{
    FactoryWorldGenerationConfig config;
    FactoryWorld *world=factory_world_create_with_seed(256U,256U,UINT64_C(9001));
    factory_world_generation_default_config(&config);
    CHECK(factory_world_generate(world,&config)==FACTORY_RESULT_OK);
    CHECK(factory_world_generation_checksum(world)!=0U);
    factory_world_destroy(world);
}

int main(void)
{
    test_seed_terrain_and_content();test_transactional_construction();
    test_snapshot_and_presentation();test_generation();
    test_generation_failure_is_transactional();
    test_moderate_world();
    if(failures!=0)return 1;
    puts("procedural world tests passed");return 0;
}
