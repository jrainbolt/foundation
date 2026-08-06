#include "foundation/content.h"

static const FactoryEntityDefinition entities[] = {
    {FACTORY_ENTITY_TYPE_EXTRACTOR,FACTORY_CONSTRUCTION_COST_EXTRACTOR,1U,1U,0,0U,FACTORY_CONTENT_RECIPE_FAMILY_NONE,1U,0U,0U},
    {FACTORY_ENTITY_TYPE_BELT,FACTORY_CONSTRUCTION_COST_BELT,1U,1U,0,0U,FACTORY_CONTENT_RECIPE_FAMILY_NONE,0U,0U,0U},
    {FACTORY_ENTITY_TYPE_REFINERY,FACTORY_CONSTRUCTION_COST_REFINERY,1U,1U,0,0U,FACTORY_CONTENT_RECIPE_FAMILY_REFINERY,1U,0U,0U},
    {FACTORY_ENTITY_TYPE_ASSEMBLER,FACTORY_CONSTRUCTION_COST_ASSEMBLER,1U,1U,0,0U,FACTORY_CONTENT_RECIPE_FAMILY_ASSEMBLER,1U,0U,0U},
    {FACTORY_ENTITY_TYPE_STORAGE,FACTORY_CONSTRUCTION_COST_STORAGE,1U,1U,FACTORY_CONTENT_ORIENTATION_NONE,0U,FACTORY_CONTENT_RECIPE_FAMILY_NONE,0U,0U,0U},
    {FACTORY_ENTITY_TYPE_SPLITTER,FACTORY_CONSTRUCTION_COST_SPLITTER,1U,1U,0,0U,FACTORY_CONTENT_RECIPE_FAMILY_NONE,0U,0U,0U},
    {FACTORY_ENTITY_TYPE_INSERTER,FACTORY_CONSTRUCTION_COST_INSERTER,1U,1U,0,0U,FACTORY_CONTENT_RECIPE_FAMILY_NONE,1U,0U,0U},
    {FACTORY_ENTITY_TYPE_POWER_POLE,FACTORY_CONSTRUCTION_COST_POWER_POLE,1U,1U,FACTORY_CONTENT_ORIENTATION_NONE,0U,FACTORY_CONTENT_RECIPE_FAMILY_NONE,4U,0U,0U},
    {FACTORY_ENTITY_TYPE_POWER_GENERATOR,FACTORY_CONSTRUCTION_COST_POWER_GENERATOR,1U,1U,FACTORY_CONTENT_ORIENTATION_NONE,0U,FACTORY_CONTENT_RECIPE_FAMILY_NONE,2U,0U,0U},
    {FACTORY_ENTITY_TYPE_FLUID_TANK,FACTORY_CONSTRUCTION_COST_FLUID_TANK,1U,1U,FACTORY_CONTENT_ORIENTATION_NONE,0U,FACTORY_CONTENT_RECIPE_FAMILY_NONE,0U,1U,0U},
    {FACTORY_ENTITY_TYPE_PIPE,FACTORY_CONSTRUCTION_COST_PIPE,1U,1U,FACTORY_CONTENT_ORIENTATION_NONE,0U,FACTORY_CONTENT_RECIPE_FAMILY_NONE,0U,2U,0U},
    {FACTORY_ENTITY_TYPE_WATER_EXTRACTOR,FACTORY_CONSTRUCTION_COST_WATER_EXTRACTOR,1U,1U,FACTORY_CONTENT_ORIENTATION_NONE,0U,FACTORY_CONTENT_RECIPE_FAMILY_FLUID,0U,4U,0U},
    {FACTORY_ENTITY_TYPE_BOILER,FACTORY_CONSTRUCTION_COST_BOILER,1U,1U,FACTORY_CONTENT_ORIENTATION_NONE,0U,FACTORY_CONTENT_RECIPE_FAMILY_FLUID,0U,12U,0U},
    {FACTORY_ENTITY_TYPE_STEAM_ENGINE,FACTORY_CONSTRUCTION_COST_STEAM_ENGINE,1U,1U,FACTORY_CONTENT_ORIENTATION_NONE,0U,FACTORY_CONTENT_RECIPE_FAMILY_STEAM,2U,8U,0U},
    {FACTORY_ENTITY_TYPE_SOLAR_GENERATOR,FACTORY_CONSTRUCTION_COST_SOLAR_GENERATOR,1U,1U,FACTORY_CONTENT_ORIENTATION_NONE,0U,FACTORY_CONTENT_RECIPE_FAMILY_NONE,2U,0U,0U},
    {FACTORY_ENTITY_TYPE_ACCUMULATOR,FACTORY_CONSTRUCTION_COST_ACCUMULATOR,1U,1U,FACTORY_CONTENT_ORIENTATION_NONE,0U,FACTORY_CONTENT_RECIPE_FAMILY_NONE,8U,0U,0U},
    {FACTORY_ENTITY_TYPE_REACTOR_CORE,FACTORY_CONSTRUCTION_COST_REACTOR_CORE,1U,1U,FACTORY_CONTENT_ORIENTATION_NONE,0U,FACTORY_CONTENT_RECIPE_FAMILY_NONE,0U,0U,2U},
    {FACTORY_ENTITY_TYPE_HEAT_CONDUCTOR,FACTORY_CONSTRUCTION_COST_HEAT_CONDUCTOR,1U,1U,FACTORY_CONTENT_ORIENTATION_NONE,0U,FACTORY_CONTENT_RECIPE_FAMILY_NONE,0U,0U,1U},
    {FACTORY_ENTITY_TYPE_HEAT_EXCHANGER,FACTORY_CONSTRUCTION_COST_HEAT_EXCHANGER,1U,1U,FACTORY_CONTENT_ORIENTATION_NONE,0U,FACTORY_CONTENT_RECIPE_FAMILY_FLUID,0U,12U,4U},
    {FACTORY_ENTITY_TYPE_STEAM_TURBINE,FACTORY_CONSTRUCTION_COST_STEAM_TURBINE,1U,1U,FACTORY_CONTENT_ORIENTATION_NONE,0U,FACTORY_CONTENT_RECIPE_FAMILY_STEAM,2U,12U,0U},
    {FACTORY_ENTITY_TYPE_STEAM_CONDENSER,FACTORY_CONSTRUCTION_COST_STEAM_CONDENSER,1U,1U,FACTORY_CONTENT_ORIENTATION_NONE,FACTORY_UNLOCK_FLUID_HANDLING,FACTORY_CONTENT_RECIPE_FAMILY_FLUID,1U,12U,0U}
};

static const FactoryRefineryRecipeDefinition refinery_recipes[] = {
    {FACTORY_RECIPE_IRON_PLATE,{FACTORY_ITEM_IRON_ORE,1U,
        FACTORY_ITEM_IRON_PLATE,1U,FACTORY_IRON_PLATE_PROCESSING_TICKS}},
    {FACTORY_RECIPE_COPPER_PLATE,{FACTORY_ITEM_COPPER_ORE,1U,
        FACTORY_ITEM_COPPER_PLATE,1U,FACTORY_IRON_PLATE_PROCESSING_TICKS}}
};

static const FactoryAssemblerRecipe assembler_recipes[] = {
    {FACTORY_ASSEMBLER_RECIPE_ELECTRONIC_COMPONENT,
        {FACTORY_ITEM_IRON_PLATE,FACTORY_ITEM_COPPER_PLATE},{1U,1U},2U,
        FACTORY_ITEM_ELECTRONIC_COMPONENT,1U,
        FACTORY_ASSEMBLER_ELECTRONIC_COMPONENT_TICKS,0U},
    {FACTORY_ASSEMBLER_RECIPE_IRON_GEAR,
        {FACTORY_ITEM_IRON_PLATE,FACTORY_ITEM_NONE},{2U,0U},1U,
        FACTORY_ITEM_IRON_GEAR,1U,FACTORY_ASSEMBLER_ELECTRONIC_COMPONENT_TICKS,0U},
    {FACTORY_ASSEMBLER_RECIPE_COPPER_WIRE,
        {FACTORY_ITEM_COPPER_PLATE,FACTORY_ITEM_NONE},{1U,0U},1U,
        FACTORY_ITEM_COPPER_WIRE,2U,FACTORY_ASSEMBLER_ELECTRONIC_COMPONENT_TICKS,
        FACTORY_UNLOCK_AUTOMATION}
};

static const FactoryTechnologyDefinition technologies[] = {
    {FACTORY_TECHNOLOGY_BASIC_AUTOMATION,{0U,0U},0U,
        FACTORY_ITEM_BASIC_SCIENCE,2U,2U,3U,FACTORY_UNLOCK_AUTOMATION},
    {FACTORY_TECHNOLOGY_FLUID_HANDLING,
        {FACTORY_TECHNOLOGY_BASIC_AUTOMATION,0U},1U,
        FACTORY_ITEM_BASIC_SCIENCE,1U,2U,2U,FACTORY_UNLOCK_FLUID_HANDLING}
};

static const FactoryFuelDefinition fuels[] = {{
    FACTORY_ITEM_BIOMASS_PELLET,1000U,100000U,FACTORY_FUEL_CLASS_SOLID}};
static const FactoryFluidDefinition fluids[] = {
    {FACTORY_FLUID_WATER,"water",FACTORY_FLUID_CLASS_AQUEOUS},
    {FACTORY_FLUID_STEAM,"steam",FACTORY_FLUID_CLASS_VAPOR},
    {FACTORY_FLUID_EXHAUST_STEAM,"exhaust steam",FACTORY_FLUID_CLASS_VAPOR}};
static const FactoryNuclearFuelDefinition nuclear_fuels[] = {{
    FACTORY_NUCLEAR_FUEL_BASIC_ROD,UINT64_C(10000),100U,
    FACTORY_REACTOR_MAX_HEAT_OUTPUT_PER_TICK}};
static const FactorySteamGenerationRecipe steam_recipes[] = {{
    FACTORY_STEAM_GENERATION_RECIPE_BASIC,FACTORY_FLUID_STEAM,100U,100U,
    FACTORY_STEAM_ENGINE_MAX_OUTPUT}};
static const FactoryFluidConversionRecipe fluid_conversion_recipes[]={{
    FACTORY_FLUID_RECIPE_BOIL_WATER,FACTORY_FLUID_WATER,100U,100U,
    FACTORY_FLUID_STEAM,100U}};
static const FactoryHeatExchangeRecipe heat_exchange_recipes[]={{
    FACTORY_HEAT_EXCHANGE_RECIPE_WATER_TO_STEAM,UINT64_C(100),100U,100U,1U}};
static const FactorySteamTurbineDefinition steam_turbines[]={{
    FACTORY_STEAM_TURBINE_DEFINITION_BASIC,FACTORY_FLUID_STEAM,
    FACTORY_FLUID_EXHAUST_STEAM,100U,100U,200U,1U,
    FACTORY_STEAM_TURBINE_MAX_OUTPUT,FACTORY_STEAM_TURBINE_STORAGE_CAPACITY,
    FACTORY_STEAM_TURBINE_STORAGE_CAPACITY,
    FACTORY_CONSTRUCTION_COST_STEAM_TURBINE}};
static const FactorySteamCondenserDefinition steam_condensers[]={{
    FACTORY_STEAM_CONDENSER_DEFINITION_BASIC,FACTORY_FLUID_EXHAUST_STEAM,
    FACTORY_FLUID_WATER,100U,100U,50U,1U,
    FACTORY_STEAM_CONDENSER_STEAM_CAPACITY,
    FACTORY_STEAM_CONDENSER_WATER_CAPACITY,
    FACTORY_CONSTRUCTION_COST_STEAM_CONDENSER}};

#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
static const FactoryContentView content={
    entities,COUNT(entities),refinery_recipes,COUNT(refinery_recipes),
    assembler_recipes,COUNT(assembler_recipes),technologies,COUNT(technologies),
    fuels,COUNT(fuels),fluids,COUNT(fluids),nuclear_fuels,COUNT(nuclear_fuels),
    steam_recipes,COUNT(steam_recipes),
    fluid_conversion_recipes,COUNT(fluid_conversion_recipes),
    heat_exchange_recipes,COUNT(heat_exchange_recipes),
    steam_turbines,COUNT(steam_turbines),
    steam_condensers,COUNT(steam_condensers)};

const FactoryContentView *factory_content_get(void) { return &content; }
#define LOOKUPS(NAME,TYPE,FIELD,IDTYPE,ARRAY) \
size_t factory_content_##NAME##_count(void){return COUNT(ARRAY);} \
const TYPE *factory_content_##NAME##_at(size_t i){return i<COUNT(ARRAY)?&ARRAY[i]:NULL;} \
const TYPE *factory_content_##NAME##_get(IDTYPE id){for(size_t i=0U;i<COUNT(ARRAY);++i)if(ARRAY[i].FIELD==id)return &ARRAY[i];return NULL;}
LOOKUPS(entity_definition,FactoryEntityDefinition,entity_type,FactoryEntityType,entities)
LOOKUPS(refinery_recipe,FactoryRefineryRecipeDefinition,recipe_id,FactoryRecipeId,refinery_recipes)
LOOKUPS(assembler_recipe,FactoryAssemblerRecipe,recipe_id,FactoryAssemblerRecipeId,assembler_recipes)
LOOKUPS(technology,FactoryTechnologyDefinition,id,FactoryTechnologyId,technologies)
LOOKUPS(fuel,FactoryFuelDefinition,item_type,FactoryItemType,fuels)
LOOKUPS(fluid,FactoryFluidDefinition,fluid_type,FactoryFluidType,fluids)
LOOKUPS(nuclear_fuel,FactoryNuclearFuelDefinition,fuel_id,FactoryNuclearFuelId,nuclear_fuels)
LOOKUPS(steam_recipe,FactorySteamGenerationRecipe,recipe_id,FactorySteamGenerationRecipeId,steam_recipes)
LOOKUPS(fluid_conversion_recipe,FactoryFluidConversionRecipe,recipe_id,
    FactoryFluidRecipeId,fluid_conversion_recipes)
LOOKUPS(heat_exchange_recipe,FactoryHeatExchangeRecipe,recipe_id,uint32_t,
    heat_exchange_recipes)
LOOKUPS(steam_turbine,FactorySteamTurbineDefinition,definition_id,
    FactorySteamTurbineDefinitionId,steam_turbines)
LOOKUPS(steam_condenser,FactorySteamCondenserDefinition,definition_id,
    FactorySteamCondenserDefinitionId,steam_condensers)
#undef LOOKUPS

FactoryUnlockFlags factory_content_entity_unlock_requirement(FactoryEntityType id)
{const FactoryEntityDefinition*d=factory_content_entity_definition_get(id);return d!=NULL?d->required_unlock:FACTORY_UNLOCK_ALL;}
bool factory_simulation_is_entity_unlocked(const FactorySimulation*s,FactoryEntityType id)
{const FactoryEntityDefinition*d=factory_content_entity_definition_get(id);return d!=NULL&&(d->required_unlock==0U||factory_simulation_has_unlock(s,d->required_unlock));}
FactoryUnlockFlags factory_content_assembler_recipe_unlock_requirement(FactoryAssemblerRecipeId id)
{if(id==FACTORY_ASSEMBLER_RECIPE_NONE)return 0U;const FactoryAssemblerRecipe*d=factory_content_assembler_recipe_get(id);return d!=NULL?d->required_unlock:FACTORY_UNLOCK_ALL;}
bool factory_simulation_is_assembler_recipe_unlocked(const FactorySimulation*s,FactoryAssemblerRecipeId id)
{if(id==FACTORY_ASSEMBLER_RECIPE_NONE)return true;const FactoryAssemblerRecipe*d=factory_content_assembler_recipe_get(id);return d!=NULL&&(d->required_unlock==0U||factory_simulation_has_unlock(s,d->required_unlock));}

static bool item_valid(FactoryItemType item)
{ return item>FACTORY_ITEM_NONE && item<=FACTORY_ITEM_BASIC_SCIENCE; }
static bool fluid_exists(const FactoryContentView *v,FactoryFluidType id)
{ for(size_t i=0U;i<v->fluid_count;++i)if(v->fluids[i].fluid_type==id)return true;return false; }

bool factory_content_validate_view(const FactoryContentView *v)
{
    uint64_t unlocks=0U;
    if(v==NULL||v->entities==NULL||v->entity_count==0U
        ||v->entity_count!=(size_t)FACTORY_ENTITY_TYPE_STEAM_CONDENSER
        ||v->refinery_recipes==NULL||v->refinery_recipe_count==0U
        ||v->assembler_recipe_count==0U||v->technology_count==0U
        ||v->fuel_count==0U||v->fluid_count==0U||v->nuclear_fuel_count==0U
        ||v->steam_recipe_count==0U
        ||v->fluid_conversion_recipe_count==0U
        ||v->heat_exchange_recipe_count==0U||v->steam_turbine_count==0U
        ||v->steam_condenser_count==0U
        ||v->assembler_recipes==NULL||v->technologies==NULL||v->fuels==NULL
        ||v->fluids==NULL||v->nuclear_fuels==NULL||v->steam_recipes==NULL
        ||v->fluid_conversion_recipes==NULL||v->heat_exchange_recipes==NULL
        ||v->steam_turbines==NULL||v->steam_condensers==NULL)
        return false;
    for(size_t i=0U;i<v->entity_count;++i){const FactoryEntityDefinition*d=&v->entities[i];
        if(d->entity_type<=FACTORY_ENTITY_TYPE_NONE
            ||d->entity_type>FACTORY_ENTITY_TYPE_STEAM_CONDENSER
            ||d->construction_cost==0U||d->footprint_width==0U
            ||d->footprint_height==0U||(d->required_unlock&~FACTORY_UNLOCK_ALL)!=0U
            ||d->default_orientation<FACTORY_CONTENT_ORIENTATION_NONE
            ||d->default_orientation>3
            ||d->recipe_family>FACTORY_CONTENT_RECIPE_FAMILY_STEAM
            ||(d->power_roles&~15U)!=0U||(d->fluid_roles&~15U)!=0U
            ||(d->heat_roles&~7U)!=0U)return false;
        for(size_t j=i+1U;j<v->entity_count;++j)
            if(d->entity_type==v->entities[j].entity_type)return false;}
    for(size_t i=0U;i<v->refinery_recipe_count;++i){const FactoryRefineryRecipeDefinition*d=&v->refinery_recipes[i];
        if(d->recipe_id==FACTORY_RECIPE_NONE||!item_valid(d->recipe.input_item)
            ||!item_valid(d->recipe.output_item)||d->recipe.input_amount==0U
            ||d->recipe.output_amount==0U||d->recipe.processing_ticks==0U)return false;
        for(size_t j=i+1U;j<v->refinery_recipe_count;++j)if(d->recipe_id==v->refinery_recipes[j].recipe_id)return false;}
    for(size_t i=0U;i<v->assembler_recipe_count;++i){const FactoryAssemblerRecipe*d=&v->assembler_recipes[i];
        if(d->recipe_id==FACTORY_ASSEMBLER_RECIPE_NONE||d->input_count==0U
            ||d->input_count>FACTORY_ASSEMBLER_MAX_INPUT_TYPES||!item_valid(d->output_item)
            ||d->output_amount==0U||d->processing_ticks==0U
            ||(d->required_unlock&~FACTORY_UNLOCK_ALL)!=0U)return false;
        for(size_t k=0U;k<d->input_count;++k)if(!item_valid(d->input_items[k])||d->input_amounts[k]==0U)return false;
        for(size_t j=i+1U;j<v->assembler_recipe_count;++j)if(d->recipe_id==v->assembler_recipes[j].recipe_id)return false;}
    if(!factory_technology_definitions_validate(v->technologies,v->technology_count))return false;
    for(size_t i=0U;i<v->technology_count;++i){if((unlocks&v->technologies[i].unlock_flags)!=0U)return false;unlocks|=v->technologies[i].unlock_flags;}
    for(size_t i=0U;i<v->entity_count;++i)
        if((v->entities[i].required_unlock&~unlocks)!=0U)return false;
    for(size_t i=0U;i<v->assembler_recipe_count;++i)
        if((v->assembler_recipes[i].required_unlock&~unlocks)!=0U)return false;
    for(size_t i=0U;i<v->fuel_count;++i){if(!factory_fuel_definition_is_valid(&v->fuels[i]))return false;for(size_t j=i+1U;j<v->fuel_count;++j)if(v->fuels[i].item_type==v->fuels[j].item_type)return false;}
    for(size_t i=0U;i<v->fluid_count;++i){if(!factory_fluid_definition_is_valid(&v->fluids[i]))return false;for(size_t j=i+1U;j<v->fluid_count;++j)if(v->fluids[i].fluid_type==v->fluids[j].fluid_type)return false;}
    for(size_t i=0U;i<v->nuclear_fuel_count;++i){const FactoryNuclearFuelDefinition*d=&v->nuclear_fuels[i];if(d->fuel_id==FACTORY_NUCLEAR_FUEL_NONE||d->total_heat_yield==0U||d->burn_duration_ticks==0U||d->maximum_heat_output_per_tick==0U)return false;for(size_t j=i+1U;j<v->nuclear_fuel_count;++j)if(d->fuel_id==v->nuclear_fuels[j].fuel_id)return false;}
    for(size_t i=0U;i<v->steam_recipe_count;++i){const FactorySteamGenerationRecipe*d=&v->steam_recipes[i];if(!factory_steam_generation_recipe_is_valid(d)||!fluid_exists(v,d->input_fluid))return false;for(size_t j=i+1U;j<v->steam_recipe_count;++j)if(d->recipe_id==v->steam_recipes[j].recipe_id)return false;}
    for(size_t i=0U;i<v->fluid_conversion_recipe_count;++i){const FactoryFluidConversionRecipe*d=&v->fluid_conversion_recipes[i];if(d->recipe_id==FACTORY_FLUID_RECIPE_NONE||!fluid_exists(v,d->input_fluid)||!fluid_exists(v,d->output_fluid)||d->input_fluid==d->output_fluid||d->input_quantity==0U||d->output_quantity==0U||d->energy==0U)return false;for(size_t j=i+1U;j<v->fluid_conversion_recipe_count;++j)if(d->recipe_id==v->fluid_conversion_recipes[j].recipe_id)return false;}
    for(size_t i=0U;i<v->heat_exchange_recipe_count;++i){const FactoryHeatExchangeRecipe*d=&v->heat_exchange_recipes[i];if(d->recipe_id==0U||d->heat_input==0U||d->water_input==0U||d->steam_output==0U||d->maximum_cycles_per_tick==0U)return false;for(size_t j=i+1U;j<v->heat_exchange_recipe_count;++j)if(d->recipe_id==v->heat_exchange_recipes[j].recipe_id)return false;}
    for(size_t i=0U;i<v->steam_turbine_count;++i){if(!factory_steam_turbine_definition_is_valid(&v->steam_turbines[i])||!fluid_exists(v,v->steam_turbines[i].input_fluid)||!fluid_exists(v,v->steam_turbines[i].exhaust_fluid))return false;for(size_t j=i+1U;j<v->steam_turbine_count;++j)if(v->steam_turbines[i].definition_id==v->steam_turbines[j].definition_id)return false;}
    for(size_t i=0U;i<v->steam_condenser_count;++i){if(!factory_steam_condenser_definition_is_valid(&v->steam_condensers[i])||!fluid_exists(v,v->steam_condensers[i].input_fluid)||!fluid_exists(v,v->steam_condensers[i].output_fluid))return false;for(size_t j=i+1U;j<v->steam_condenser_count;++j)if(v->steam_condensers[i].definition_id==v->steam_condensers[j].definition_id)return false;}
    return true;
}

bool factory_content_validate(void){return factory_content_validate_view(&content);}

#define CATEGORY_VALIDATOR(NAME,TYPE,FIELD,COUNT_FIELD) \
bool factory_content_##NAME##_validate(const TYPE *values,size_t count){ \
    FactoryContentView view=content;view.FIELD=values;view.COUNT_FIELD=count; \
    return factory_content_validate_view(&view);}
CATEGORY_VALIDATOR(entity_definitions,FactoryEntityDefinition,entities,entity_count)
CATEGORY_VALIDATOR(refinery_recipes,FactoryRefineryRecipeDefinition,
    refinery_recipes,refinery_recipe_count)
CATEGORY_VALIDATOR(assembler_recipes,FactoryAssemblerRecipe,
    assembler_recipes,assembler_recipe_count)
CATEGORY_VALIDATOR(technologies,FactoryTechnologyDefinition,
    technologies,technology_count)
CATEGORY_VALIDATOR(fuels,FactoryFuelDefinition,fuels,fuel_count)
CATEGORY_VALIDATOR(fluids,FactoryFluidDefinition,fluids,fluid_count)
CATEGORY_VALIDATOR(nuclear_fuels,FactoryNuclearFuelDefinition,
    nuclear_fuels,nuclear_fuel_count)
CATEGORY_VALIDATOR(steam_recipes,FactorySteamGenerationRecipe,
    steam_recipes,steam_recipe_count)
#undef CATEGORY_VALIDATOR
#undef COUNT
