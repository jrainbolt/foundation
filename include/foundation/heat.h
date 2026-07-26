#ifndef FOUNDATION_HEAT_H
#define FOUNDATION_HEAT_H

#include <stdbool.h>
#include <stdint.h>

typedef uint64_t FactoryHeatQuantity;

typedef struct {
    FactoryHeatQuantity stored_heat;
    FactoryHeatQuantity capacity;
} FactoryHeatStorage;

/* HeatStorage is authoritative integer state; operations never clamp or lose heat. */
bool factory_heat_storage_is_valid(const FactoryHeatStorage *storage);
FactoryHeatQuantity factory_heat_storage_remaining_capacity(
    const FactoryHeatStorage *storage);
bool factory_heat_storage_add(
    FactoryHeatStorage *storage, FactoryHeatQuantity quantity);

#endif
