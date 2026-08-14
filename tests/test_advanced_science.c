#include "foundation/content.h"
#include "foundation/snapshot.h"
#include "logistics_endpoint_internal.h"
#include "simulation_internal.h"
#include "power_fixture.h"

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

static void test_catalog_and_physical_chain(void)
{
    FactoryWorld *world=factory_world_create(12U,6U);
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
    FactorySimulation *loaded=NULL;

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
    CHECK(factory_test_submit_power_row(s,12U,4U));
    ticks(s,1U);
    submit(s,(FactoryCommand){FACTORY_COMMAND_SET_REFINERY_RECIPE,
        {.set_refinery_recipe={1U,FACTORY_RECIPE_STEEL}}});
    submit(s,(FactoryCommand){FACTORY_COMMAND_SET_ASSEMBLER_RECIPE,
        {.set_assembler_recipe={2U,
            FACTORY_ASSEMBLER_RECIPE_ADVANCED_COMPONENT}}});
    submit(s,(FactoryCommand){FACTORY_COMMAND_SET_ASSEMBLER_RECIPE,
        {.set_assembler_recipe={3U,
            FACTORY_ASSEMBLER_RECIPE_ADVANCED_SCIENCE}}});
    ticks(s,1U);
    CHECK(factory_logistics_endpoint_insert(s,
        (FactoryLogisticsEndpoint){1U,FACTORY_LOGISTICS_SLOT_INPUT},
        FACTORY_ITEM_IRON_PLATE)==FACTORY_LOGISTICS_RESULT_OK);
    CHECK(factory_logistics_endpoint_insert(s,
        (FactoryLogisticsEndpoint){1U,FACTORY_LOGISTICS_SLOT_INPUT},
        FACTORY_ITEM_IRON_PLATE)==FACTORY_LOGISTICS_RESULT_OK);
    CHECK(factory_logistics_endpoint_insert(s,
        (FactoryLogisticsEndpoint){1U,FACTORY_LOGISTICS_SLOT_INPUT},
        FACTORY_ITEM_COAL)==FACTORY_LOGISTICS_RESULT_OK);
    ticks(s,20U);
    CHECK(factory_simulation_get_refinery(s,1U,&refinery)
        &&refinery.output_item==FACTORY_ITEM_STEEL
        &&refinery.output_amount==1U);
    CHECK(factory_logistics_endpoint_transfer(s,
        (FactoryLogisticsEndpoint){1U,FACTORY_LOGISTICS_SLOT_OUTPUT},
        (FactoryLogisticsEndpoint){2U,FACTORY_LOGISTICS_SLOT_ASSEMBLER_INPUT_0},
        FACTORY_ITEM_STEEL)==FACTORY_LOGISTICS_RESULT_OK);
    for(uint32_t i=0U;i<2U;++i)CHECK(factory_logistics_endpoint_insert(s,
        (FactoryLogisticsEndpoint){2U,FACTORY_LOGISTICS_SLOT_ASSEMBLER_INPUT_1},
        FACTORY_ITEM_COPPER_WIRE)==FACTORY_LOGISTICS_RESULT_OK);
    ticks(s,20U);
    CHECK(factory_simulation_get_assembler(s,2U,&assembler)
        &&assembler.output_item==FACTORY_ITEM_ADVANCED_COMPONENT);
    CHECK(factory_logistics_endpoint_transfer(s,
        (FactoryLogisticsEndpoint){2U,FACTORY_LOGISTICS_SLOT_OUTPUT},
        (FactoryLogisticsEndpoint){3U,FACTORY_LOGISTICS_SLOT_ASSEMBLER_INPUT_0},
        FACTORY_ITEM_ADVANCED_COMPONENT)==FACTORY_LOGISTICS_RESULT_OK);
    CHECK(factory_logistics_endpoint_insert(s,
        (FactoryLogisticsEndpoint){3U,FACTORY_LOGISTICS_SLOT_ASSEMBLER_INPUT_1},
        FACTORY_ITEM_ELECTRONIC_COMPONENT)==FACTORY_LOGISTICS_RESULT_OK);
    ticks(s,25U);
    CHECK(factory_simulation_get_assembler(s,3U,&assembler)
        &&assembler.output_item==FACTORY_ITEM_ADVANCED_SCIENCE);
    CHECK(factory_logistics_endpoint_transfer(s,
        (FactoryLogisticsEndpoint){3U,FACTORY_LOGISTICS_SLOT_OUTPUT},
        (FactoryLogisticsEndpoint){4U,FACTORY_LOGISTICS_SLOT_RESEARCH_LAB_INPUT},
        FACTORY_ITEM_ADVANCED_SCIENCE)==FACTORY_LOGISTICS_RESULT_OK);
    CHECK(factory_simulation_get_research_lab(s,4U,&lab)==FACTORY_RESULT_OK
        &&lab.science_item==FACTORY_ITEM_ADVANCED_SCIENCE
        &&lab.science_quantity==1U);
    for(uint32_t i=0U;i<5U;++i)CHECK(factory_logistics_endpoint_insert(s,
        (FactoryLogisticsEndpoint){4U,FACTORY_LOGISTICS_SLOT_RESEARCH_LAB_INPUT},
        FACTORY_ITEM_ADVANCED_SCIENCE)==FACTORY_LOGISTICS_RESULT_OK);
    submit(s,(FactoryCommand){FACTORY_COMMAND_SELECT_RESEARCH,
        {.select_research={FACTORY_TECHNOLOGY_ADVANCED_MANUFACTURING}}});
    ticks(s,9U);
    CHECK(factory_simulation_is_technology_completed(s,
        FACTORY_TECHNOLOGY_ADVANCED_MANUFACTURING));
    CHECK(factory_simulation_has_unlock(s,
        FACTORY_UNLOCK_ADVANCED_MANUFACTURING));
    CHECK(factory_simulation_get_research_lab(s,4U,&lab)==FACTORY_RESULT_OK
        &&lab.science_item==FACTORY_ITEM_NONE&&lab.science_quantity==0U);
    CHECK(factory_simulation_create_snapshot(s,&bytes)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_load_snapshot(bytes.data,bytes.size,&loaded)
        ==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_research_lab(loaded,4U,&lab)==FACTORY_RESULT_OK
        &&lab.science_item==FACTORY_ITEM_NONE&&lab.science_quantity==0U);
    factory_snapshot_buffer_destroy(&bytes);
    factory_simulation_destroy(loaded);
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

int main(void)
{
    test_catalog_and_physical_chain();
    test_finite_coal_depletion();
    if(failures!=0)return 1;
    puts("All advanced science tests passed.");
    return 0;
}
