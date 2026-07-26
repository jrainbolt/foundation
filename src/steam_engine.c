#include "steam_engine_internal.h"

#include "fluid_internal.h"
#include "simulation_internal.h"

#include <stdlib.h>

static const FactorySteamGenerationRecipe recipes[] = {{
    FACTORY_STEAM_GENERATION_RECIPE_BASIC,
    FACTORY_FLUID_STEAM,
    100U,
    100U,
    FACTORY_STEAM_ENGINE_MAX_OUTPUT
}};

size_t factory_steam_generation_recipe_count(void)
{
    return sizeof(recipes) / sizeof(recipes[0]);
}

bool factory_steam_generation_recipe_is_valid(
    const FactorySteamGenerationRecipe *recipe
)
{
    return recipe != NULL
        && recipe->recipe_id == FACTORY_STEAM_GENERATION_RECIPE_BASIC
        && recipe->input_fluid == FACTORY_FLUID_STEAM
        && recipe->input_quantity != 0U
        && recipe->generated_energy != 0U
        && recipe->maximum_output_per_tick != 0U
        && recipe->input_quantity == recipe->generated_energy;
}

const FactorySteamGenerationRecipe *factory_steam_generation_recipe_get(
    FactorySteamGenerationRecipeId id
)
{
    return id == FACTORY_STEAM_GENERATION_RECIPE_BASIC ? &recipes[0] : NULL;
}

static bool reserve(FactorySteamEngineStore *store)
{
    size_t next;
    FactorySteamEngine *items;
    if (store->count < store->capacity) return true;
    next = store->capacity == 0U ? 4U : store->capacity * 2U;
    if (next < store->capacity || next > SIZE_MAX / sizeof(*items))
        return false;
    items = realloc(store->items, next * sizeof(*items));
    if (items == NULL) return false;
    store->items = items;
    store->capacity = next;
    return true;
}

void factory_steam_engine_store_destroy(FactorySteamEngineStore *store)
{
    if (store == NULL) return;
    free(store->items);
    *store = (FactorySteamEngineStore){0};
}

bool factory_steam_engine_store_reserve_one(FactorySteamEngineStore *store)
{
    return store != NULL && reserve(store);
}

void factory_steam_engine_store_add(
    FactorySteamEngineStore *store,
    FactoryEntityId id,
    int32_t x,
    int32_t y
)
{
    store->items[store->count++] = (FactorySteamEngine){
        id, x, y, FACTORY_STEAM_GENERATION_RECIPE_BASIC, 0U};
}

const FactorySteamEngine *factory_steam_engine_store_find(
    const FactorySteamEngineStore *store, FactoryEntityId id
)
{
    if (store == NULL) return NULL;
    for (size_t i = 0U; i < store->count; ++i)
        if (store->items[i].entity_id == id) return &store->items[i];
    return NULL;
}

FactorySteamEngine *factory_steam_engine_store_find_mutable(
    FactorySteamEngineStore *store, FactoryEntityId id
)
{
    return (FactorySteamEngine *)factory_steam_engine_store_find(store, id);
}

bool factory_steam_engine_store_remove(
    FactorySteamEngineStore *store, FactoryEntityId id
)
{
    if (store == NULL) return false;
    for (size_t i = 0U; i < store->count; ++i)
        if (store->items[i].entity_id == id) {
            --store->count;
            store->items[i] = store->items[store->count];
            return true;
        }
    return false;
}

void factory_steam_engine_begin_tick(FactorySimulation *simulation)
{
    if (simulation == NULL) return;
    for (size_t i = 0U; i < simulation->steam_engines.count; ++i)
        simulation->steam_engines.items[i].generated_last_tick = 0U;
}

FactoryPowerUnits factory_steam_engine_available_generation(
    const FactorySimulation *simulation, FactoryEntityId id
)
{
    const FactorySteamEngine *engine;
    const FactorySteamGenerationRecipe *recipe;
    const FactoryFluidStorage *storage;
    FactoryPowerUnits available;
    if (simulation == NULL) return 0U;
    engine = factory_steam_engine_store_find(&simulation->steam_engines, id);
    if (engine == NULL) return 0U;
    recipe = factory_steam_generation_recipe_get(engine->recipe_id);
    storage = factory_fluid_storage_store_find_slot(
        &simulation->fluid_storages, id,
        FACTORY_FLUID_STORAGE_STEAM_ENGINE_INPUT);
    if (!factory_steam_generation_recipe_is_valid(recipe)
        || storage == NULL
        || (storage->quantity != 0U
            && storage->fluid_type != recipe->input_fluid))
        return 0U;
    available = (FactoryPowerUnits)storage->quantity;
    return available < recipe->maximum_output_per_tick
        ? available : recipe->maximum_output_per_tick;
}

bool factory_steam_engine_consume_for_generation(
    FactorySimulation *simulation,
    FactoryEntityId id,
    FactoryPowerUnits generated
)
{
    FactorySteamEngine *engine;
    const FactorySteamGenerationRecipe *recipe;
    FactoryFluidStorage *storage;
    if (simulation == NULL || generated == 0U) return false;
    engine = factory_steam_engine_store_find_mutable(
        &simulation->steam_engines, id);
    if (engine == NULL) return false;
    recipe = factory_steam_generation_recipe_get(engine->recipe_id);
    storage = factory_fluid_storage_store_find_slot_mutable(
        &simulation->fluid_storages, id,
        FACTORY_FLUID_STORAGE_STEAM_ENGINE_INPUT);
    if (!factory_steam_generation_recipe_is_valid(recipe)
        || generated > recipe->maximum_output_per_tick
        || storage == NULL || storage->fluid_type != recipe->input_fluid
        || storage->quantity < generated)
        return false;
    storage->quantity -= generated;
    if (storage->quantity == 0U) storage->fluid_type = FACTORY_FLUID_NONE;
    engine->generated_last_tick += generated;
    factory_simulation_emit_event(simulation, (FactoryEvent){
        .type = FACTORY_EVENT_STEAM_ENGINE_GENERATION_COMPLETED,
        .entity_id = id,
        .fluid_type = recipe->input_fluid,
        .quantity = generated,
        .related_quantity = generated});
    return true;
}

FactoryResult factory_simulation_get_steam_engine(
    const FactorySimulation *simulation,
    FactoryEntityId id,
    FactorySteamEngineInspection *out
)
{
    const FactorySteamEngine *engine;
    const FactorySteamGenerationRecipe *recipe;
    const FactoryFluidStorage *storage;
    if (simulation == NULL || id == 0U || out == NULL)
        return FACTORY_RESULT_INVALID_ARGUMENT;
    engine = factory_steam_engine_store_find(&simulation->steam_engines, id);
    if (engine == NULL) return FACTORY_RESULT_ENTITY_NOT_FOUND;
    recipe = factory_steam_generation_recipe_get(engine->recipe_id);
    storage = factory_fluid_storage_store_find_slot(
        &simulation->fluid_storages, id,
        FACTORY_FLUID_STORAGE_STEAM_ENGINE_INPUT);
    if (!factory_steam_generation_recipe_is_valid(recipe) || storage == NULL)
        return FACTORY_RESULT_INTERNAL_STATE_MISMATCH;
    *out = (FactorySteamEngineInspection){
        id, engine->recipe_id, storage->quantity, storage->capacity,
        recipe->maximum_output_per_tick, engine->generated_last_tick,
        engine->generated_last_tick != 0U};
    return FACTORY_RESULT_OK;
}
