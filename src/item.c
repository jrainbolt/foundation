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
        case FACTORY_ITEM_BIOMASS_PELLET:
            return "biomass pellet";
        case FACTORY_ITEM_BASIC_SCIENCE:
            return "basic science";
        case FACTORY_ITEM_CONSTRUCTION_MATERIAL:
            return "construction material";
        case FACTORY_ITEM_COAL:
            return "coal";
        case FACTORY_ITEM_STEEL:
            return "steel";
        case FACTORY_ITEM_ADVANCED_COMPONENT:
            return "advanced component";
        case FACTORY_ITEM_ADVANCED_SCIENCE:
            return "advanced science";
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
        case FACTORY_ITEM_STEEL:
        case FACTORY_ITEM_ADVANCED_COMPONENT:
        case FACTORY_ITEM_ADVANCED_SCIENCE:
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
        case FACTORY_ITEM_ADVANCED_COMPONENT:
        case FACTORY_ITEM_ADVANCED_SCIENCE:
            return 2U;
        default:
            return 0U;
    }
}
