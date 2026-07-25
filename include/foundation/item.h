#ifndef FOUNDATION_ITEM_H
#define FOUNDATION_ITEM_H

#include <stdint.h>

#define FACTORY_ELEMENT_UNIT_SCALE 2U

typedef enum {
    FACTORY_ITEM_NONE = 0,
    FACTORY_ITEM_IRON_ORE,
    FACTORY_ITEM_IRON_PLATE,
    FACTORY_ITEM_COPPER_ORE,
    FACTORY_ITEM_COPPER_PLATE,
    FACTORY_ITEM_ELECTRONIC_COMPONENT,
    FACTORY_ITEM_IRON_GEAR,
    FACTORY_ITEM_COPPER_WIRE
} FactoryItemType;

/* Returns a stable name, or "invalid item" for an unknown value. */
const char *factory_item_name(FactoryItemType item);

/* Elemental content in half-unit scale, used for exact conservation checks. */
uint32_t factory_item_iron_units(FactoryItemType item);
uint32_t factory_item_copper_units(FactoryItemType item);

#endif
