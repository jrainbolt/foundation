#include "fluid_machine_internal.h"

#include "burner_internal.h"
#include "fluid_internal.h"
#include "simulation_internal.h"

#include <stdlib.h>

static const FactoryFluidConversionRecipe recipes[] = {
    {
        FACTORY_FLUID_RECIPE_BOIL_WATER,
        FACTORY_FLUID_WATER, 100U, 100U,
        FACTORY_FLUID_STEAM, 100U
    }
};

size_t factory_fluid_conversion_recipe_count(void)
{
    return sizeof(recipes) / sizeof(recipes[0]);
}

bool factory_fluid_conversion_recipe_is_valid(
    const FactoryFluidConversionRecipe *r
)
{
    return r != NULL && r->recipe_id == FACTORY_FLUID_RECIPE_BOIL_WATER
        && factory_fluid_definition_get(r->input_fluid) != NULL
        && factory_fluid_definition_get(r->output_fluid) != NULL
        && r->input_fluid != r->output_fluid
        && r->input_quantity != 0U && r->output_quantity != 0U
        && r->energy != 0U;
}

const FactoryFluidConversionRecipe *factory_fluid_conversion_recipe_get(
    FactoryFluidRecipeId id
)
{
    return id == FACTORY_FLUID_RECIPE_BOIL_WATER ? &recipes[0] : NULL;
}

static bool reserve(void **items, size_t *capacity, size_t count, size_t width)
{
    size_t next;
    void *p;
    if (count < *capacity) return true;
    next = *capacity == 0U ? 4U : *capacity * 2U;
    if (next < *capacity || next > SIZE_MAX / width) return false;
    p = realloc(*items, next * width);
    if (p == NULL) return false;
    *items = p; *capacity = next; return true;
}

#define DESTROY(NAME, TYPE) void NAME(TYPE *s) { if (s != NULL) {           \
    free(s->items); *s = (TYPE){0}; } }
DESTROY(factory_water_extractor_store_destroy, FactoryWaterExtractorStore)
DESTROY(factory_boiler_store_destroy, FactoryBoilerStore)

bool factory_water_extractor_store_reserve_one(FactoryWaterExtractorStore *s)
{
    return s != NULL && reserve((void **)&s->items, &s->capacity, s->count,
        sizeof(*s->items));
}
bool factory_boiler_store_reserve_one(FactoryBoilerStore *s)
{
    return s != NULL && reserve((void **)&s->items, &s->capacity, s->count,
        sizeof(*s->items));
}
void factory_water_extractor_store_add(
    FactoryWaterExtractorStore *s, FactoryEntityId id, int32_t x, int32_t y
)
{
    s->items[s->count++] = (FactoryWaterExtractor){id, x, y, 0U};
}
void factory_boiler_store_add(
    FactoryBoilerStore *s, FactoryEntityId id, int32_t x, int32_t y
)
{
    s->items[s->count++] = (FactoryBoiler){
        id, x, y, FACTORY_FLUID_RECIPE_BOIL_WATER, false};
}

#define FIND(NAME, TYPE, STORE) const TYPE *NAME(const STORE *s,             \
    FactoryEntityId id) {                                                    \
    if (s == NULL) return NULL;                                              \
    for (size_t i = 0U; i < s->count; ++i) {                                \
        if (s->items[i].entity_id == id) return &s->items[i];                \
    }                                                                        \
    return NULL;                                                             \
}
FIND(factory_water_extractor_store_find, FactoryWaterExtractor,
    FactoryWaterExtractorStore)
FIND(factory_boiler_store_find, FactoryBoiler, FactoryBoilerStore)

#define REMOVE(NAME, STORE) bool NAME(STORE *s, FactoryEntityId id) {        \
    if (s == NULL) return false;                                             \
    for (size_t i = 0U; i < s->count; ++i) {                                \
        if (s->items[i].entity_id == id) {                                   \
            --s->count;                                                      \
            s->items[i] = s->items[s->count];                               \
            return true;                                                     \
        }                                                                    \
    }                                                                        \
    return false;                                                            \
}
REMOVE(factory_water_extractor_store_remove, FactoryWaterExtractorStore)
REMOVE(factory_boiler_store_remove, FactoryBoilerStore)

void factory_fluid_machines_update(FactorySimulation *s)
{
    for (size_t i = 0U; i < s->water_extractors.count; ++i) {
        FactoryWaterExtractor *m = &s->water_extractors.items[i];
        FactoryFluidStorage *out = factory_fluid_storage_store_find_slot_mutable(
            &s->fluid_storages, m->entity_id, FACTORY_FLUID_STORAGE_DEFAULT);
        if (out == NULL) continue;
        if (m->progress + 1U < FACTORY_WATER_EXTRACTOR_CYCLE_TICKS) {
            ++m->progress; continue;
        }
        if (factory_fluid_storage_insert(
                out, FACTORY_FLUID_WATER,
                FACTORY_WATER_EXTRACTOR_OUTPUT_QUANTITY)
            != FACTORY_RESULT_OK) continue;
        m->progress = 0U;
        factory_simulation_emit_event(s, (FactoryEvent){
            .type = FACTORY_EVENT_WATER_PRODUCED,
            .entity_id = m->entity_id,
            .fluid_type = FACTORY_FLUID_WATER,
            .quantity = FACTORY_WATER_EXTRACTOR_OUTPUT_QUANTITY});
    }
    for (size_t i = 0U; i < s->boilers.count; ++i) {
        FactoryBoiler *m = &s->boilers.items[i];
        const FactoryFluidConversionRecipe *r =
            factory_fluid_conversion_recipe_get(m->recipe_id);
        FactoryFluidStorage *in = factory_fluid_storage_store_find_slot_mutable(
            &s->fluid_storages, m->entity_id,
            FACTORY_FLUID_STORAGE_BOILER_INPUT);
        FactoryFluidStorage *out = factory_fluid_storage_store_find_slot_mutable(
            &s->fluid_storages, m->entity_id,
            FACTORY_FLUID_STORAGE_BOILER_OUTPUT);
        FactoryBurner *burner =
            factory_burner_store_find_mutable(&s->burners, m->entity_id);
        m->conversion_active = false;
        if (!factory_fluid_conversion_recipe_is_valid(r) || in == NULL
            || out == NULL || burner == NULL
            || in->fluid_type != r->input_fluid
            || in->quantity < r->input_quantity
            || burner->released_energy < r->energy
            || (out->quantity != 0U && out->fluid_type != r->output_fluid)
            || r->output_quantity > out->capacity - out->quantity) continue;
        /* All validation precedes this atomic, allocation-free commit. */
        (void)factory_burner_consume_energy(burner, r->energy);
        in->quantity -= r->input_quantity;
        if (in->quantity == 0U) in->fluid_type = FACTORY_FLUID_NONE;
        out->fluid_type = r->output_fluid;
        out->quantity += r->output_quantity;
        m->conversion_active = true;
        factory_simulation_emit_event(s, (FactoryEvent){
            .type = FACTORY_EVENT_BOILER_CONVERSION_COMPLETED,
            .entity_id = m->entity_id,
            .fluid_type = r->input_fluid,
            .related_fluid_type = r->output_fluid,
            .quantity = r->input_quantity,
            .related_quantity = r->output_quantity});
    }
}

FactoryResult factory_simulation_get_water_extractor(
    const FactorySimulation *s, FactoryEntityId id,
    FactoryWaterExtractorInspection *out
)
{
    const FactoryWaterExtractor *m;
    const FactoryFluidStorage *storage;
    if (s == NULL || id == 0U || out == NULL)
        return FACTORY_RESULT_INVALID_ARGUMENT;
    m = factory_water_extractor_store_find(&s->water_extractors, id);
    storage = factory_fluid_storage_store_find_slot(
        &s->fluid_storages, id, FACTORY_FLUID_STORAGE_DEFAULT);
    if (m == NULL || storage == NULL) return FACTORY_RESULT_ENTITY_NOT_FOUND;
    *out = (FactoryWaterExtractorInspection){
        id, m->progress, FACTORY_WATER_EXTRACTOR_CYCLE_TICKS,
        storage->quantity, storage->capacity};
    return FACTORY_RESULT_OK;
}

FactoryResult factory_simulation_get_boiler(
    const FactorySimulation *s, FactoryEntityId id, FactoryBoilerInspection *out
)
{
    const FactoryBoiler *m;
    const FactoryFluidStorage *in, *steam;
    if (s == NULL || id == 0U || out == NULL)
        return FACTORY_RESULT_INVALID_ARGUMENT;
    m = factory_boiler_store_find(&s->boilers, id);
    in = factory_fluid_storage_store_find_slot(
        &s->fluid_storages, id, FACTORY_FLUID_STORAGE_BOILER_INPUT);
    steam = factory_fluid_storage_store_find_slot(
        &s->fluid_storages, id, FACTORY_FLUID_STORAGE_BOILER_OUTPUT);
    if (m == NULL || in == NULL || steam == NULL)
        return FACTORY_RESULT_ENTITY_NOT_FOUND;
    *out = (FactoryBoilerInspection){
        id, m->recipe_id, in->quantity, in->capacity,
        steam->quantity, steam->capacity, m->conversion_active};
    return FACTORY_RESULT_OK;
}
