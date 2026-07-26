#include "foundation/entity.h"
#include "entity_internal.h"

#include <stddef.h>
#include <stdlib.h>

FactoryEntityManager *factory_entity_manager_create(void)
{
    FactoryEntityManager *manager = calloc(1U, sizeof(*manager));

    if (manager != NULL) {
        manager->next_id = 1U;
    }
    return manager;
}

void factory_entity_manager_destroy(FactoryEntityManager *manager)
{
    if (manager == NULL) {
        return;
    }
    free(manager->live_ids);
    free(manager);
}

FactoryEntityId factory_entity_create(FactoryEntityManager *manager)
{
    FactoryEntityId *resized_ids = NULL;
    size_t new_capacity = 0U;
    FactoryEntityId id = 0U;

    if (manager == NULL || manager->next_id == 0U) {
        return 0U;
    }
    if (manager->count == manager->capacity) {
        new_capacity = manager->capacity == 0U ? 8U : manager->capacity * 2U;
        if (new_capacity < manager->capacity
            || new_capacity > SIZE_MAX / sizeof(*manager->live_ids)) {
            return 0U;
        }
        resized_ids = realloc(
            manager->live_ids,
            new_capacity * sizeof(*manager->live_ids)
        );
        if (resized_ids == NULL) {
            return 0U;
        }
        manager->live_ids = resized_ids;
        manager->capacity = new_capacity;
    }

    id = manager->next_id;
    ++manager->next_id;
    manager->live_ids[manager->count] = id;
    ++manager->count;
    return id;
}

bool factory_entity_is_valid(
    const FactoryEntityManager *manager,
    FactoryEntityId id
)
{
    size_t index = 0U;

    if (manager == NULL || id == 0U) {
        return false;
    }
    for (index = 0U; index < manager->count; ++index) {
        if (manager->live_ids[index] == id) {
            return true;
        }
    }
    return false;
}

void factory_entity_destroy(
    FactoryEntityManager *manager,
    FactoryEntityId id
)
{
    size_t index = 0U;

    if (manager == NULL || id == 0U) {
        return;
    }
    for (index = 0U; index < manager->count; ++index) {
        if (manager->live_ids[index] == id) {
            --manager->count;
            manager->live_ids[index] = manager->live_ids[manager->count];
            return;
        }
    }
}

size_t factory_entity_get_count(const FactoryEntityManager *manager)
{
    return manager == NULL ? 0U : manager->count;
}
