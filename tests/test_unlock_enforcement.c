#include "foundation/content.h"
#include "foundation/simulation.h"
#include <stdbool.h>
#include <stdio.h>
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#c);++failures;}}while(false)
static void submit(FactorySimulation*s,FactoryCommand c){CHECK(factory_simulation_submit_command(s,&c)==FACTORY_RESULT_OK);}
static void tick(FactorySimulation*s){CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);}
static void unlock(FactorySimulation*s,FactoryTechnologyId id,uint32_t q,uint32_t n){
 submit(s,(FactoryCommand){FACTORY_COMMAND_INSERT_RESEARCH_SCIENCE,{.insert_research_science={q}}});
 submit(s,(FactoryCommand){FACTORY_COMMAND_SELECT_RESEARCH,{.select_research={id}}});
 for(uint32_t i=0;i<n;++i)tick(s);
}
int main(void){
 FactoryWorld*w=factory_world_create(8,8); FactorySimulation*s=factory_simulation_create_with_construction_units(w,500);
 FactoryCommand c={FACTORY_COMMAND_PLACE_STEAM_CONDENSER,{.place_steam_condenser={1,1}}};
 FactoryConstructionMaterial before=factory_simulation_construction_units(s);
 CHECK(factory_content_entity_unlock_requirement(FACTORY_ENTITY_TYPE_STEAM_CONDENSER)==FACTORY_UNLOCK_FLUID_HANDLING);
 CHECK(factory_content_assembler_recipe_unlock_requirement(FACTORY_ASSEMBLER_RECIPE_COPPER_WIRE)==FACTORY_UNLOCK_AUTOMATION);
 submit(s,c);tick(s); CHECK(factory_simulation_get_command_result(s,0)->result==FACTORY_RESULT_TECHNOLOGY_LOCKED);
 CHECK(factory_simulation_get_entity_count(s)==0&&factory_simulation_construction_units(s)==before&&factory_simulation_get_event_count(s)==0);
 unlock(s,FACTORY_TECHNOLOGY_BASIC_AUTOMATION,4,6); unlock(s,FACTORY_TECHNOLOGY_FLUID_HANDLING,2,4);
 CHECK(factory_simulation_is_entity_unlocked(s,FACTORY_ENTITY_TYPE_STEAM_CONDENSER)); submit(s,c);tick(s);
 CHECK(factory_simulation_get_command_result(s,0)->result==FACTORY_RESULT_OK);
 factory_simulation_destroy(s);factory_world_destroy(w);
 w=factory_world_create(4,4);s=factory_simulation_create_with_construction_units(w,500);
 submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_ASSEMBLER,{.place_assembler={0,0,FACTORY_DIRECTION_EAST}}});tick(s);
 submit(s,(FactoryCommand){FACTORY_COMMAND_SET_ASSEMBLER_RECIPE,{.set_assembler_recipe={1,FACTORY_ASSEMBLER_RECIPE_COPPER_WIRE}}});tick(s);
 CHECK(factory_simulation_get_command_result(s,0)->result==FACTORY_RESULT_TECHNOLOGY_LOCKED);
 FactoryAssembler a;CHECK(factory_simulation_get_assembler(s,1,&a)&&a.recipe_id==FACTORY_ASSEMBLER_RECIPE_NONE);
 unlock(s,FACTORY_TECHNOLOGY_BASIC_AUTOMATION,4,6);
 submit(s,(FactoryCommand){FACTORY_COMMAND_SET_ASSEMBLER_RECIPE,{.set_assembler_recipe={1,FACTORY_ASSEMBLER_RECIPE_COPPER_WIRE}}});tick(s);
 CHECK(factory_simulation_get_command_result(s,0)->result==FACTORY_RESULT_OK);
 factory_simulation_destroy(s);factory_world_destroy(w);return failures?1:0;
}
