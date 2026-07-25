#include "construction_inventory_internal.h"

#include <assert.h>
#include <stdint.h>

bool factory_construction_inventory_can_spend(
    const FactoryConstructionInventory *inventory,
    FactoryConstructionMaterial amount
)
{
    return inventory != NULL && inventory->units >= amount;
}

bool factory_construction_inventory_can_credit(
    const FactoryConstructionInventory *inventory,
    FactoryConstructionMaterial amount
)
{
    return inventory != NULL && amount <= UINT32_MAX - inventory->units;
}

void factory_construction_inventory_spend_validated(
    FactoryConstructionInventory *inventory,
    FactoryConstructionMaterial amount
)
{
    assert(factory_construction_inventory_can_spend(inventory, amount));
    inventory->units -= amount;
}

void factory_construction_inventory_credit_validated(
    FactoryConstructionInventory *inventory,
    FactoryConstructionMaterial amount
)
{
    assert(factory_construction_inventory_can_credit(inventory, amount));
    inventory->units += amount;
}
