#include <foundation/content.h>
#include <foundation/presentation.h>
#include <foundation/snapshot.h>

#include <stdbool.h>
#include <stdio.h>

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

int main(void)
{
    test_seed_terrain_and_content();test_transactional_construction();
    test_snapshot_and_presentation();
    if(failures!=0)return 1;
    puts("procedural world tests passed");return 0;
}
