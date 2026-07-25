#ifndef FOUNDATION_CONSTRUCTION_INVENTORY_INTERNAL_H
#define FOUNDATION_CONSTRUCTION_INVENTORY_INTERNAL_H

#include <stdbool.h>

#include "foundation/construction.h"

typedef struct {
    FactoryConstructionMaterial units;
} FactoryConstructionInventory;

bool factory_construction_inventory_can_spend(
    const FactoryConstructionInventory *inventory,
    FactoryConstructionMaterial amount
);
bool factory_construction_inventory_can_credit(
    const FactoryConstructionInventory *inventory,
    FactoryConstructionMaterial amount
);
void factory_construction_inventory_spend_validated(
    FactoryConstructionInventory *inventory,
    FactoryConstructionMaterial amount
);
void factory_construction_inventory_credit_validated(
    FactoryConstructionInventory *inventory,
    FactoryConstructionMaterial amount
);

#endif
