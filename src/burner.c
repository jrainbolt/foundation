#include "burner_internal.h"

#include "event_internal.h"
#include "simulation_internal.h"

#include <stdlib.h>

static const FactoryFuelDefinition fuel_definitions[] = {
    {
        FACTORY_ITEM_BIOMASS_PELLET,
        1000U,
        100000U,
        FACTORY_FUEL_CLASS_SOLID
    }
};

size_t factory_fuel_definition_count(void)
{
    return sizeof(fuel_definitions) / sizeof(fuel_definitions[0]);
}

const FactoryFuelDefinition *factory_fuel_definition_at(size_t index)
{
    return index < factory_fuel_definition_count()
        ? &fuel_definitions[index] : NULL;
}

bool factory_fuel_definition_is_valid(
    const FactoryFuelDefinition *definition
)
{
    return definition != NULL
        && definition->item_type > FACTORY_ITEM_NONE
        && definition->item_type <= FACTORY_ITEM_BIOMASS_PELLET
        && definition->burn_duration_ticks != 0U
        && definition->energy != 0U
        && definition->energy <= FACTORY_BURNER_RELEASED_ENERGY_CAPACITY
        && definition->fuel_class != 0U;
}

const FactoryFuelDefinition *factory_fuel_definition_get(
    FactoryItemType item
)
{
    size_t index;
    for (index = 0U; index < factory_fuel_definition_count(); ++index)
        if (fuel_definitions[index].item_type == item
            && factory_fuel_definition_is_valid(&fuel_definitions[index]))
            return &fuel_definitions[index];
    return NULL;
}

void factory_burner_store_destroy(FactoryBurnerStore *store)
{
    if (store == NULL) return;
    free(store->items);
    *store = (FactoryBurnerStore){0};
}

bool factory_burner_store_reserve_one(FactoryBurnerStore *store)
{
    FactoryBurner *items;
    size_t capacity;
    if (store == NULL) return false;
    if (store->count < store->capacity) return true;
    capacity = store->capacity == 0U ? 4U : store->capacity * 2U;
    if (capacity < store->capacity
        || capacity > SIZE_MAX / sizeof(*items)) return false;
    items = realloc(store->items, capacity * sizeof(*items));
    if (items == NULL) return false;
    store->items = items;
    store->capacity = capacity;
    return true;
}

void factory_burner_store_add(
    FactoryBurnerStore *store,
    FactoryEntityId owner,
    FactoryFuelClassMask accepted_classes
)
{
    store->items[store->count++] = (FactoryBurner){
        .owner_entity_id = owner,
        .accepted_fuel_classes = accepted_classes
    };
}

const FactoryBurner *factory_burner_store_find(
    const FactoryBurnerStore *store, FactoryEntityId owner
)
{
    size_t index;
    if (store == NULL) return NULL;
    for (index = 0U; index < store->count; ++index)
        if (store->items[index].owner_entity_id == owner)
            return &store->items[index];
    return NULL;
}

FactoryBurner *factory_burner_store_find_mutable(
    FactoryBurnerStore *store, FactoryEntityId owner
)
{
    return (FactoryBurner *)factory_burner_store_find(store, owner);
}

bool factory_burner_store_remove(
    FactoryBurnerStore *store, FactoryEntityId owner
)
{
    size_t index;
    if (store == NULL) return false;
    for (index = 0U; index < store->count; ++index) {
        if (store->items[index].owner_entity_id == owner) {
            --store->count;
            store->items[index] = store->items[store->count];
            return true;
        }
    }
    return false;
}

bool factory_burner_can_accept(
    const FactoryBurner *burner, FactoryItemType item
)
{
    const FactoryFuelDefinition *definition =
        factory_fuel_definition_get(item);
    return burner != NULL && definition != NULL
        && (burner->accepted_fuel_classes & definition->fuel_class) != 0U
        && burner->inventory_quantity != UINT32_MAX
        && (burner->inventory_item == FACTORY_ITEM_NONE
            || burner->inventory_item == item);
}

bool factory_burner_insert(FactoryBurner *burner, FactoryItemType item)
{
    if (!factory_burner_can_accept(burner, item)) return false;
    burner->inventory_item = item;
    ++burner->inventory_quantity;
    return true;
}

bool factory_burner_consume_energy(
    FactoryBurner *burner, FactoryEnergy amount
)
{
    if (burner == NULL || amount > burner->released_energy) return false;
    burner->released_energy -= amount;
    return true;
}

bool factory_fuel_release_for_tick(
    const FactoryFuelDefinition *definition,
    uint32_t elapsed_ticks,
    FactoryEnergy *out_release
)
{
    FactoryEnergy base;
    FactoryEnergy remainder;
    if (!factory_fuel_definition_is_valid(definition)
        || out_release == NULL
        || elapsed_ticks >= definition->burn_duration_ticks)
        return false;
    base = definition->energy / definition->burn_duration_ticks;
    remainder = definition->energy % definition->burn_duration_ticks;
    *out_release = base + (elapsed_ticks < remainder ? 1U : 0U);
    return true;
}

FactoryEnergy factory_burner_unreleased_energy(
    const FactoryBurner *burner
)
{
    const FactoryFuelDefinition *definition;
    uint32_t elapsed;
    FactoryEnergy base;
    FactoryEnergy remainder;
    FactoryEnergy released;
    if (burner == NULL || burner->remaining_burn_ticks == 0U)
        return 0U;
    definition = factory_fuel_definition_get(burner->current_fuel_item);
    if (definition == NULL
        || burner->remaining_burn_ticks > definition->burn_duration_ticks)
        return 0U;
    elapsed =
        definition->burn_duration_ticks - burner->remaining_burn_ticks;
    base = definition->energy / definition->burn_duration_ticks;
    remainder = definition->energy % definition->burn_duration_ticks;
    released = base * elapsed
        + (elapsed < remainder ? elapsed : remainder);
    return definition->energy - released;
}

void factory_burner_store_begin_tick(
    FactoryBurnerStore *store, FactorySimulation *simulation
)
{
    size_t index;
    for (index = 0U; index < store->count; ++index) {
        FactoryBurner *burner = &store->items[index];
        if (burner->remaining_burn_ticks == 0U
            && burner->inventory_quantity != 0U) {
            const FactoryFuelDefinition *definition =
                factory_fuel_definition_get(burner->inventory_item);
            if (definition != NULL
                && burner->released_energy
                    <= FACTORY_BURNER_RELEASED_ENERGY_CAPACITY
                        - definition->energy) {
                burner->current_fuel_item = burner->inventory_item;
                burner->remaining_burn_ticks =
                    definition->burn_duration_ticks;
                --burner->inventory_quantity;
                if (burner->inventory_quantity == 0U)
                    burner->inventory_item = FACTORY_ITEM_NONE;
                factory_simulation_emit_event(simulation, (FactoryEvent){
                    .type = FACTORY_EVENT_FUEL_IGNITED,
                    .entity_id = burner->owner_entity_id,
                    .item_type = burner->current_fuel_item,
                    .quantity = 1U
                });
            }
        }
        if (burner->remaining_burn_ticks != 0U) {
            const FactoryFuelDefinition *definition =
                factory_fuel_definition_get(burner->current_fuel_item);
            uint32_t elapsed = definition->burn_duration_ticks
                - burner->remaining_burn_ticks;
            FactoryEnergy release = 0U;
            (void)factory_fuel_release_for_tick(
                definition, elapsed, &release);
            burner->released_energy += release;
        }
    }
}

void factory_burner_store_finish_tick(
    FactoryBurnerStore *store, FactorySimulation *simulation
)
{
    size_t index;
    for (index = 0U; index < store->count; ++index) {
        FactoryBurner *burner = &store->items[index];
        if (burner->remaining_burn_ticks != 0U
            && --burner->remaining_burn_ticks == 0U) {
            factory_simulation_emit_event(simulation, (FactoryEvent){
                .type = FACTORY_EVENT_FUEL_EXHAUSTED,
                .entity_id = burner->owner_entity_id,
                .item_type = burner->current_fuel_item,
                .quantity = 1U
            });
            burner->current_fuel_item = FACTORY_ITEM_NONE;
        }
    }
}

FactoryResult factory_simulation_get_burner(
    const FactorySimulation *simulation,
    FactoryEntityId owner_entity_id,
    FactoryBurnerInspection *out_burner
)
{
    const FactoryBurner *burner;
    if (simulation == NULL || owner_entity_id == 0U || out_burner == NULL)
        return FACTORY_RESULT_INVALID_ARGUMENT;
    burner = factory_burner_store_find(&simulation->burners, owner_entity_id);
    if (burner == NULL) return FACTORY_RESULT_ENTITY_NOT_FOUND;
    *out_burner = (FactoryBurnerInspection){
        burner->owner_entity_id,
        burner->accepted_fuel_classes,
        burner->inventory_item,
        burner->inventory_quantity,
        burner->current_fuel_item,
        burner->remaining_burn_ticks,
        factory_fuel_definition_get(burner->current_fuel_item) == NULL
            ? 0U
            : factory_fuel_definition_get(
                burner->current_fuel_item)->burn_duration_ticks,
        factory_burner_unreleased_energy(burner),
        burner->released_energy,
        burner->remaining_burn_ticks != 0U
    };
    return FACTORY_RESULT_OK;
}
