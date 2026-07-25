#ifndef FOUNDATION_ITEM_H
#define FOUNDATION_ITEM_H

typedef enum {
    FACTORY_ITEM_NONE = 0,
    FACTORY_ITEM_IRON_ORE,
    FACTORY_ITEM_IRON_PLATE,
    FACTORY_ITEM_COPPER_ORE,
    FACTORY_ITEM_COPPER_PLATE,
    FACTORY_ITEM_ELECTRONIC_COMPONENT
} FactoryItemType;

/* Returns a stable name, or "invalid item" for an unknown value. */
const char *factory_item_name(FactoryItemType item);

#endif
