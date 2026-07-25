#include "logistics_endpoint_internal.h"

#include "simulation_internal.h"
#include "assembler_recipe_internal.h"

static bool item_is_valid(FactoryItemType item)
{
    return item > FACTORY_ITEM_NONE
        && item <= FACTORY_ITEM_COPPER_WIRE;
}

bool factory_logistics_endpoint_equal(
    FactoryLogisticsEndpoint left,
    FactoryLogisticsEndpoint right
)
{
    return left.entity_id == right.entity_id && left.slot == right.slot;
}

static FactoryLogisticsResult validate_entity(
    const FactorySimulation *simulation,
    FactoryLogisticsEndpoint endpoint
)
{
    if (simulation == NULL
        || endpoint.entity_id == 0U
        || !factory_entity_is_valid(
            simulation->entities, endpoint.entity_id)) {
        return FACTORY_LOGISTICS_RESULT_INVALID_ENTITY;
    }
    return FACTORY_LOGISTICS_RESULT_OK;
}

FactoryLogisticsResult factory_logistics_endpoint_peek(
    const FactorySimulation *simulation,
    FactoryLogisticsEndpoint endpoint,
    FactoryItemType *out_item
)
{
    const FactoryExtractor *extractor;
    const FactoryBelt *belt;
    const FactorySplitter *splitter;
    const FactoryRefinery *refinery;
    const FactoryAssembler *assembler;
    const FactoryStorage *storage;
    const FactoryInserter *inserter;
    FactoryLogisticsResult result = validate_entity(simulation, endpoint);

    if (result != FACTORY_LOGISTICS_RESULT_OK) {
        return result;
    }
    if (out_item == NULL) {
        return FACTORY_LOGISTICS_RESULT_STATE_MISMATCH;
    }
    extractor = factory_extractor_store_find(
        &simulation->extractors, endpoint.entity_id
    );
    if (extractor != NULL) {
        if (endpoint.slot != FACTORY_LOGISTICS_SLOT_OUTPUT) {
            return FACTORY_LOGISTICS_RESULT_INVALID_SLOT;
        }
        if (extractor->output_item == FACTORY_ITEM_NONE
            || extractor->output_amount == 0U) {
            return FACTORY_LOGISTICS_RESULT_EMPTY;
        }
        *out_item = extractor->output_item;
        return FACTORY_LOGISTICS_RESULT_OK;
    }
    belt = factory_belt_store_find(&simulation->belts, endpoint.entity_id);
    if (belt != NULL) {
        if (endpoint.slot != FACTORY_LOGISTICS_SLOT_MAIN) {
            return FACTORY_LOGISTICS_RESULT_INVALID_SLOT;
        }
        if (belt->item == FACTORY_ITEM_NONE) {
            return FACTORY_LOGISTICS_RESULT_EMPTY;
        }
        *out_item = belt->item;
        return FACTORY_LOGISTICS_RESULT_OK;
    }
    splitter = factory_splitter_store_find(
        &simulation->splitters, endpoint.entity_id
    );
    if (splitter != NULL) {
        if (endpoint.slot != FACTORY_LOGISTICS_SLOT_SPLITTER_LEFT_OUTPUT
            && endpoint.slot
                != FACTORY_LOGISTICS_SLOT_SPLITTER_RIGHT_OUTPUT) {
            return FACTORY_LOGISTICS_RESULT_INVALID_SLOT;
        }
        if (splitter->item == FACTORY_ITEM_NONE) {
            return FACTORY_LOGISTICS_RESULT_EMPTY;
        }
        *out_item = splitter->item;
        return FACTORY_LOGISTICS_RESULT_OK;
    }
    refinery = factory_refinery_store_find(
        &simulation->refineries, endpoint.entity_id
    );
    if (refinery != NULL) {
        if (endpoint.slot != FACTORY_LOGISTICS_SLOT_OUTPUT) {
            return FACTORY_LOGISTICS_RESULT_INVALID_SLOT;
        }
        if (refinery->output_item == FACTORY_ITEM_NONE
            || refinery->output_amount == 0U) {
            return FACTORY_LOGISTICS_RESULT_EMPTY;
        }
        *out_item = refinery->output_item;
        return FACTORY_LOGISTICS_RESULT_OK;
    }
    assembler = factory_assembler_store_find(
        &simulation->assemblers, endpoint.entity_id
    );
    if (assembler != NULL) {
        if (endpoint.slot != FACTORY_LOGISTICS_SLOT_OUTPUT) {
            return FACTORY_LOGISTICS_RESULT_INVALID_SLOT;
        }
        if (assembler->output_item == FACTORY_ITEM_NONE
            || assembler->output_amount == 0U) {
            return FACTORY_LOGISTICS_RESULT_EMPTY;
        }
        *out_item = assembler->output_item;
        return FACTORY_LOGISTICS_RESULT_OK;
    }
    storage = factory_storage_store_find(
        &simulation->storages, endpoint.entity_id
    );
    if (storage != NULL) {
        if (endpoint.slot != FACTORY_LOGISTICS_SLOT_STORAGE_OUTPUT) {
            return FACTORY_LOGISTICS_RESULT_INVALID_SLOT;
        }
        if (!storage->output_occupied
            || storage->output_item == FACTORY_ITEM_NONE) {
            return FACTORY_LOGISTICS_RESULT_EMPTY;
        }
        *out_item = storage->output_item;
        return FACTORY_LOGISTICS_RESULT_OK;
    }
    inserter = factory_inserter_store_find(
        &simulation->inserters, endpoint.entity_id
    );
    if (inserter != NULL) {
        if (endpoint.slot != FACTORY_LOGISTICS_SLOT_INSERTER_HELD) {
            return FACTORY_LOGISTICS_RESULT_INVALID_SLOT;
        }
        if (inserter->held_item == FACTORY_ITEM_NONE
            || inserter->held_amount == 0U) {
            return FACTORY_LOGISTICS_RESULT_EMPTY;
        }
        *out_item = inserter->held_item;
        return FACTORY_LOGISTICS_RESULT_OK;
    }
    return FACTORY_LOGISTICS_RESULT_INVALID_SLOT;
}

FactoryLogisticsResult factory_logistics_endpoint_can_accept(
    const FactorySimulation *simulation,
    FactoryLogisticsEndpoint endpoint,
    FactoryItemType item
)
{
    const FactoryBelt *belt;
    const FactorySplitter *splitter;
    const FactoryRefinery *refinery;
    const FactoryAssembler *assembler;
    const FactoryInserter *inserter;
    const FactoryStorage *storage;
    const FactoryRecipe *recipe;
    FactoryLogisticsResult result = validate_entity(simulation, endpoint);

    if (result != FACTORY_LOGISTICS_RESULT_OK) {
        return result;
    }
    if (!item_is_valid(item)) {
        return FACTORY_LOGISTICS_RESULT_INVALID_ITEM;
    }
    belt = factory_belt_store_find(&simulation->belts, endpoint.entity_id);
    if (belt != NULL) {
        if (endpoint.slot != FACTORY_LOGISTICS_SLOT_MAIN) {
            return FACTORY_LOGISTICS_RESULT_INVALID_SLOT;
        }
        return belt->item == FACTORY_ITEM_NONE
            ? FACTORY_LOGISTICS_RESULT_OK
            : FACTORY_LOGISTICS_RESULT_BLOCKED;
    }
    splitter = factory_splitter_store_find(
        &simulation->splitters, endpoint.entity_id
    );
    if (splitter != NULL) {
        if (endpoint.slot != FACTORY_LOGISTICS_SLOT_SPLITTER_INPUT) {
            return FACTORY_LOGISTICS_RESULT_INVALID_SLOT;
        }
        return splitter->item == FACTORY_ITEM_NONE
            ? FACTORY_LOGISTICS_RESULT_OK
            : FACTORY_LOGISTICS_RESULT_BLOCKED;
    }
    refinery = factory_refinery_store_find(
        &simulation->refineries, endpoint.entity_id
    );
    if (refinery != NULL) {
        if (endpoint.slot != FACTORY_LOGISTICS_SLOT_INPUT) {
            return FACTORY_LOGISTICS_RESULT_INVALID_SLOT;
        }
        recipe = factory_recipe_get(refinery->recipe_id);
        if (recipe == NULL || recipe->input_item != item) {
            return FACTORY_LOGISTICS_RESULT_INCOMPATIBLE_ITEM;
        }
        return refinery->input_item == FACTORY_ITEM_NONE
            && refinery->input_amount == 0U
            ? FACTORY_LOGISTICS_RESULT_OK
            : FACTORY_LOGISTICS_RESULT_BLOCKED;
    }
    assembler = factory_assembler_store_find(
        &simulation->assemblers, endpoint.entity_id
    );
    if (assembler != NULL) {
        size_t slot;
        const FactoryAssemblerRecipe *assembler_recipe =
            factory_assembler_recipe_find(assembler->recipe_id);

        if (endpoint.slot == FACTORY_LOGISTICS_SLOT_ASSEMBLER_INPUT_0) {
            slot = 0U;
        } else if (endpoint.slot
            == FACTORY_LOGISTICS_SLOT_ASSEMBLER_INPUT_1) {
            slot = 1U;
        } else {
            return FACTORY_LOGISTICS_RESULT_INVALID_SLOT;
        }
        if (assembler_recipe == NULL
            || slot >= assembler_recipe->input_count
            || assembler->input_slots[slot].capacity == 0U) {
            return FACTORY_LOGISTICS_RESULT_INCOMPATIBLE_ITEM;
        }
        if (assembler_recipe->input_items[slot] != item) {
            return FACTORY_LOGISTICS_RESULT_INCOMPATIBLE_ITEM;
        }
        return assembler->input_slots[slot].count
                < assembler->input_slots[slot].capacity
            ? FACTORY_LOGISTICS_RESULT_OK
            : FACTORY_LOGISTICS_RESULT_BLOCKED;
    }
    inserter = factory_inserter_store_find(
        &simulation->inserters, endpoint.entity_id
    );
    if (inserter != NULL) {
        if (endpoint.slot != FACTORY_LOGISTICS_SLOT_INSERTER_HELD) {
            return FACTORY_LOGISTICS_RESULT_INVALID_SLOT;
        }
        if (inserter->state != FACTORY_INSERTER_STATE_PICKING_UP) {
            return FACTORY_LOGISTICS_RESULT_STATE_MISMATCH;
        }
        return inserter->held_item == FACTORY_ITEM_NONE
            && inserter->held_amount == 0U
            ? FACTORY_LOGISTICS_RESULT_OK
            : FACTORY_LOGISTICS_RESULT_BLOCKED;
    }
    storage = factory_storage_store_find(
        &simulation->storages, endpoint.entity_id
    );
    if (storage != NULL) {
        if (endpoint.slot != FACTORY_LOGISTICS_SLOT_STORAGE_INPUT) {
            return FACTORY_LOGISTICS_RESULT_INVALID_SLOT;
        }
        return factory_storage_get_total_amount(storage)
                < storage->total_capacity
            ? FACTORY_LOGISTICS_RESULT_OK
            : FACTORY_LOGISTICS_RESULT_BLOCKED;
    }
    return FACTORY_LOGISTICS_RESULT_INVALID_SLOT;
}

static FactoryLogisticsResult validate_removal(
    const FactorySimulation *simulation,
    FactoryLogisticsEndpoint endpoint,
    FactoryItemType expected_item
)
{
    FactoryItemType item;
    FactoryLogisticsResult result;
    const FactoryInserter *inserter;

    if (!item_is_valid(expected_item)) {
        return FACTORY_LOGISTICS_RESULT_INVALID_ITEM;
    }
    result = factory_logistics_endpoint_peek(simulation, endpoint, &item);
    if (result != FACTORY_LOGISTICS_RESULT_OK) {
        return result;
    }
    if (item != expected_item) {
        return FACTORY_LOGISTICS_RESULT_STATE_MISMATCH;
    }
    inserter = factory_inserter_store_find(
        &simulation->inserters, endpoint.entity_id
    );
    if (inserter != NULL
        && inserter->state != FACTORY_INSERTER_STATE_DROPPING) {
        return FACTORY_LOGISTICS_RESULT_STATE_MISMATCH;
    }
    return FACTORY_LOGISTICS_RESULT_OK;
}

static void remove_unchecked(
    FactorySimulation *simulation,
    FactoryLogisticsEndpoint endpoint
)
{
    FactoryExtractor *extractor = factory_extractor_store_find_mutable(
        &simulation->extractors, endpoint.entity_id
    );
    FactoryBelt *belt = factory_belt_store_find_mutable(
        &simulation->belts, endpoint.entity_id
    );
    FactorySplitter *splitter = factory_splitter_store_find_mutable(
        &simulation->splitters, endpoint.entity_id
    );
    FactoryRefinery *refinery = factory_refinery_store_find_mutable(
        &simulation->refineries, endpoint.entity_id
    );
    FactoryAssembler *assembler = factory_assembler_store_find_mutable(
        &simulation->assemblers, endpoint.entity_id
    );
    FactoryStorage *storage = factory_storage_store_find_mutable(
        &simulation->storages, endpoint.entity_id
    );
    FactoryInserter *inserter = factory_inserter_store_find_mutable(
        &simulation->inserters, endpoint.entity_id
    );

    if (extractor != NULL) {
        extractor->output_item = FACTORY_ITEM_NONE;
        extractor->output_amount = 0U;
    } else if (belt != NULL) {
        belt->item = FACTORY_ITEM_NONE;
        belt->movement_progress = 0U;
    } else if (splitter != NULL) {
        splitter->item = FACTORY_ITEM_NONE;
    } else if (refinery != NULL) {
        refinery->output_item = FACTORY_ITEM_NONE;
        refinery->output_amount = 0U;
    } else if (assembler != NULL) {
        --assembler->output_amount;
        if (assembler->output_amount == 0U) {
            assembler->output_item = FACTORY_ITEM_NONE;
        }
    } else if (storage != NULL) {
        storage->output_item = FACTORY_ITEM_NONE;
        storage->output_occupied = false;
    } else if (inserter != NULL) {
        inserter->held_item = FACTORY_ITEM_NONE;
        inserter->held_amount = 0U;
    }
}

FactoryLogisticsResult factory_logistics_endpoint_remove(
    FactorySimulation *simulation,
    FactoryLogisticsEndpoint endpoint,
    FactoryItemType expected_item
)
{
    FactoryLogisticsResult result = validate_removal(
        simulation, endpoint, expected_item
    );

    if (result == FACTORY_LOGISTICS_RESULT_OK) {
        remove_unchecked(simulation, endpoint);
    }
    return result;
}

static void insert_unchecked(
    FactorySimulation *simulation,
    FactoryLogisticsEndpoint endpoint,
    FactoryItemType item
)
{
    FactoryBelt *belt = factory_belt_store_find_mutable(
        &simulation->belts, endpoint.entity_id
    );
    FactorySplitter *splitter = factory_splitter_store_find_mutable(
        &simulation->splitters, endpoint.entity_id
    );
    FactoryRefinery *refinery = factory_refinery_store_find_mutable(
        &simulation->refineries, endpoint.entity_id
    );
    FactoryAssembler *assembler = factory_assembler_store_find_mutable(
        &simulation->assemblers, endpoint.entity_id
    );
    FactoryInserter *inserter = factory_inserter_store_find_mutable(
        &simulation->inserters, endpoint.entity_id
    );
    FactoryStorage *storage = factory_storage_store_find_mutable(
        &simulation->storages, endpoint.entity_id
    );

    if (belt != NULL) {
        belt->item = item;
        belt->movement_progress = 0U;
    } else if (splitter != NULL) {
        splitter->item = item;
    } else if (refinery != NULL) {
        refinery->input_item = item;
        refinery->input_amount = 1U;
    } else if (assembler != NULL) {
        size_t slot = endpoint.slot
                == FACTORY_LOGISTICS_SLOT_ASSEMBLER_INPUT_0
            ? 0U : 1U;

        ++assembler->input_slots[slot].count;
    } else if (inserter != NULL) {
        inserter->held_item = item;
        inserter->held_amount = 1U;
    } else if (item == FACTORY_ITEM_IRON_ORE) {
        ++storage->iron_ore_amount;
    } else if (item == FACTORY_ITEM_IRON_PLATE) {
        ++storage->iron_plate_amount;
    } else if (item == FACTORY_ITEM_COPPER_ORE) {
        ++storage->copper_ore_amount;
    } else if (item == FACTORY_ITEM_COPPER_PLATE) {
        ++storage->copper_plate_amount;
    } else if (item == FACTORY_ITEM_ELECTRONIC_COMPONENT) {
        ++storage->electronic_component_amount;
    } else if (item == FACTORY_ITEM_IRON_GEAR) {
        ++storage->iron_gear_amount;
    } else {
        ++storage->copper_wire_amount;
    }
}

FactoryLogisticsResult factory_logistics_endpoint_insert(
    FactorySimulation *simulation,
    FactoryLogisticsEndpoint endpoint,
    FactoryItemType item
)
{
    FactoryLogisticsResult result = factory_logistics_endpoint_can_accept(
        simulation, endpoint, item
    );

    if (result == FACTORY_LOGISTICS_RESULT_OK) {
        insert_unchecked(simulation, endpoint, item);
    }
    return result;
}

FactoryLogisticsResult factory_logistics_endpoint_transfer(
    FactorySimulation *simulation,
    FactoryLogisticsEndpoint source,
    FactoryLogisticsEndpoint destination,
    FactoryItemType expected_item
)
{
    FactoryLogisticsResult result = validate_removal(
        simulation, source, expected_item
    );

    if (result != FACTORY_LOGISTICS_RESULT_OK) {
        return result;
    }
    result = factory_logistics_endpoint_can_accept(
        simulation, destination, expected_item
    );
    if (result != FACTORY_LOGISTICS_RESULT_OK) {
        return result;
    }
    /*
     * Both endpoints are fully validated before mutation. This single-threaded
     * commit phase has no callback or allocation point between these writes,
     * so insertion cannot fail after removal.
     */
    remove_unchecked(simulation, source);
    insert_unchecked(simulation, destination, expected_item);
    return FACTORY_LOGISTICS_RESULT_OK;
}
