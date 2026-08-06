#include "foundation/content.h"
#include "foundation/presentation.h"
#include "foundation/snapshot.h"
#include "foundation/simulation.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c) do {if(!(c)){(void)fprintf(stderr,"FAIL %s:%d: %s\n", \
    __FILE__,__LINE__,#c);++failures;}}while(false)

static void test_lookup_and_compatibility(void)
{
    const FactoryContentView *view=factory_content_get();
    CHECK(view!=NULL && factory_content_validate());
    CHECK(factory_content_entity_definitions_validate(
        view->entities,view->entity_count));
    CHECK(factory_content_refinery_recipes_validate(
        view->refinery_recipes,view->refinery_recipe_count));
    CHECK(factory_content_assembler_recipes_validate(
        view->assembler_recipes,view->assembler_recipe_count));
    CHECK(factory_content_technologies_validate(
        view->technologies,view->technology_count));
    CHECK(factory_content_fuels_validate(view->fuels,view->fuel_count));
    CHECK(factory_content_fluids_validate(view->fluids,view->fluid_count));
    CHECK(factory_content_nuclear_fuels_validate(
        view->nuclear_fuels,view->nuclear_fuel_count));
    CHECK(factory_content_steam_recipes_validate(
        view->steam_recipes,view->steam_recipe_count));
    CHECK(factory_content_entity_definition_count()==21U);
    for(size_t i=0U;i<view->entity_count;++i) {
        const FactoryEntityDefinition *d=factory_content_entity_definition_at(i);
        FactoryConstructionMaterial cost=0U;
        CHECK(d==&view->entities[i]);
        CHECK(i==0U || view->entities[i-1U].entity_type<d->entity_type);
        CHECK(factory_content_entity_definition_get(d->entity_type)==d);
        CHECK(factory_entity_construction_cost(d->entity_type,&cost));
        CHECK(cost==d->construction_cost);
    }
    CHECK(factory_content_entity_definition_at(view->entity_count)==NULL);
    CHECK(factory_content_entity_definition_get(FACTORY_ENTITY_TYPE_NONE)==NULL);
    for(size_t i=0U;i<view->refinery_recipe_count;++i) {
        const FactoryRefineryRecipeDefinition *d=
            factory_content_refinery_recipe_at(i);
        CHECK(factory_content_refinery_recipe_get(d->recipe_id)==d);
        CHECK(factory_recipe_get(d->recipe_id)==&d->recipe);
    }
    for(size_t i=0U;i<view->assembler_recipe_count;++i) {
        FactoryAssemblerRecipe copied={0};
        const FactoryAssemblerRecipe *d=factory_content_assembler_recipe_at(i);
        CHECK(factory_content_assembler_recipe_get(d->recipe_id)==d);
        CHECK(factory_assembler_recipe_get(d->recipe_id,&copied));
        CHECK(memcmp(&copied,d,sizeof(copied))==0);
    }
    for(size_t i=0U;i<view->technology_count;++i)
        CHECK(factory_technology_definition_at(i)==
            factory_content_technology_at(i));
    for(size_t i=0U;i<view->fuel_count;++i)
        CHECK(factory_fuel_definition_at(i)==factory_content_fuel_at(i));
    for(size_t i=0U;i<view->fluid_count;++i)
        CHECK(factory_fluid_definition_at(i)==factory_content_fluid_at(i));
    for(size_t i=0U;i<view->nuclear_fuel_count;++i)
        CHECK(factory_nuclear_fuel_definition_at(i)==
            factory_content_nuclear_fuel_at(i));
    CHECK(factory_steam_generation_recipe_get(
        FACTORY_STEAM_GENERATION_RECIPE_BASIC)==
        factory_content_steam_recipe_get(FACTORY_STEAM_GENERATION_RECIPE_BASIC));
    CHECK(factory_fluid_conversion_recipe_get(FACTORY_FLUID_RECIPE_BOIL_WATER)
        ==factory_content_fluid_conversion_recipe_get(
            FACTORY_FLUID_RECIPE_BOIL_WATER));
    CHECK(factory_heat_exchange_recipe_get(
        FACTORY_HEAT_EXCHANGE_RECIPE_WATER_TO_STEAM)
        ==factory_content_heat_exchange_recipe_get(
            FACTORY_HEAT_EXCHANGE_RECIPE_WATER_TO_STEAM));
    CHECK(factory_steam_turbine_definition_at(0U)
        ==factory_content_steam_turbine_at(0U));
    CHECK(factory_steam_condenser_definition_at(0U)
        ==factory_content_steam_condenser_at(0U));
}

static void test_invalid_views(void)
{
    const FactoryContentView *base=factory_content_get();
    FactoryContentView view=*base;
    FactoryEntityDefinition entities[21];
    FactoryRefineryRecipeDefinition refinery[2];
    FactoryTechnologyDefinition technologies[2];
    FactorySteamGenerationRecipe steam[1];
    (void)memcpy(entities,base->entities,sizeof(entities));
    entities[1].entity_type=entities[0].entity_type;
    view.entities=entities;
    CHECK(!factory_content_validate_view(&view));
    (void)memcpy(entities,base->entities,sizeof(entities));
    entities[0].required_unlock=UINT64_C(1)<<20U;
    CHECK(!factory_content_validate_view(&view));
    view=*base; (void)memcpy(refinery,base->refinery_recipes,sizeof(refinery));
    refinery[1].recipe_id=refinery[0].recipe_id; view.refinery_recipes=refinery;
    CHECK(!factory_content_validate_view(&view));
    view=*base; (void)memcpy(technologies,base->technologies,sizeof(technologies));
    technologies[1].unlock_flags=technologies[0].unlock_flags;
    view.technologies=technologies;
    CHECK(!factory_content_validate_view(&view));
    view=*base; (void)memcpy(steam,base->steam_recipes,sizeof(steam));
    steam[0].input_fluid=(FactoryFluidType)99; view.steam_recipes=steam;
    CHECK(!factory_content_validate_view(&view));
}

static void test_observer_and_snapshot_independence(void)
{
    FactoryWorld *world=factory_world_create(3U,3U);
    FactorySimulation *simulation=
        factory_simulation_create_with_construction_units(world,100U);
    FactorySnapshotBuffer before={0},after={0};
    FactoryPresentationSnapshot *a=factory_presentation_snapshot_create();
    FactoryPresentationSnapshot *b=factory_presentation_snapshot_create();
    FactoryCommand command={FACTORY_COMMAND_PLACE_STORAGE,
        {.place_storage={1,1}}};
    CHECK(simulation!=NULL);
    CHECK(factory_simulation_submit_command(simulation,&command)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(simulation)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_create_snapshot(simulation,&before)==FACTORY_RESULT_OK);
    CHECK(factory_presentation_snapshot_rebuild(a,simulation)==FACTORY_RESULT_OK);
    CHECK(factory_content_validate());
    for(size_t i=0U;i<factory_content_entity_definition_count();++i)
        CHECK(factory_content_entity_definition_at(i)!=NULL);
    CHECK(factory_presentation_snapshot_rebuild(b,simulation)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_create_snapshot(simulation,&after)==FACTORY_RESULT_OK);
    CHECK(before.size==after.size && memcmp(before.data,after.data,before.size)==0);
    CHECK(factory_presentation_snapshot_get_tick(a)==
        factory_presentation_snapshot_get_tick(b));
    CHECK(factory_presentation_snapshot_get_entity_count(a)==
        factory_presentation_snapshot_get_entity_count(b));
    CHECK(factory_presentation_snapshot_get_entity(a,0U)->entity_id==
        factory_presentation_snapshot_get_entity(b,0U)->entity_id);
    factory_presentation_snapshot_destroy(a);
    factory_presentation_snapshot_destroy(b);
    factory_snapshot_buffer_destroy(&before); factory_snapshot_buffer_destroy(&after);
    factory_simulation_destroy(simulation); factory_world_destroy(world);
}

int main(void)
{
    test_lookup_and_compatibility();
    test_invalid_views();
    test_observer_and_snapshot_independence();
    return failures==0?0:1;
}
