#include "foundation/item.h"

const char *factory_item_name(FactoryItemType item)
{
    switch (item) {
        case FACTORY_ITEM_NONE:
            return "none";
        case FACTORY_ITEM_IRON_ORE:
            return "iron ore";
        case FACTORY_ITEM_IRON_PLATE:
            return "iron plate";
        case FACTORY_ITEM_COPPER_ORE:
            return "copper ore";
        case FACTORY_ITEM_COPPER_PLATE:
            return "copper plate";
        case FACTORY_ITEM_ELECTRONIC_COMPONENT:
            return "electronic component";
        case FACTORY_ITEM_IRON_GEAR:
            return "iron gear";
        case FACTORY_ITEM_COPPER_WIRE:
            return "copper wire";
        default:
            return "invalid item";
    }
}

uint32_t factory_item_iron_units(FactoryItemType item)
{
    switch (item) {
        case FACTORY_ITEM_IRON_ORE:
        case FACTORY_ITEM_IRON_PLATE:
        case FACTORY_ITEM_ELECTRONIC_COMPONENT:
            return 2U;
        case FACTORY_ITEM_IRON_GEAR:
            return 4U;
        default:
            return 0U;
    }
}

uint32_t factory_item_copper_units(FactoryItemType item)
{
    switch (item) {
        case FACTORY_ITEM_COPPER_ORE:
        case FACTORY_ITEM_COPPER_PLATE:
        case FACTORY_ITEM_ELECTRONIC_COMPONENT:
            return 2U;
        case FACTORY_ITEM_COPPER_WIRE:
            return 1U;
        default:
            return 0U;
    }
}
