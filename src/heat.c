#include "foundation/heat.h"

#include <stddef.h>

bool factory_heat_storage_is_valid(const FactoryHeatStorage *storage)
{
    return storage != NULL && storage->capacity != 0U
        && storage->stored_heat <= storage->capacity;
}

FactoryHeatQuantity factory_heat_storage_remaining_capacity(
    const FactoryHeatStorage *storage)
{
    return factory_heat_storage_is_valid(storage)
        ? storage->capacity - storage->stored_heat : 0U;
}

bool factory_heat_storage_add(
    FactoryHeatStorage *storage, FactoryHeatQuantity quantity)
{
    if (!factory_heat_storage_is_valid(storage)
        || quantity > storage->capacity - storage->stored_heat)
        return false;
    storage->stored_heat += quantity;
    return true;
}
