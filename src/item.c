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
        default:
            return "invalid item";
    }
}
