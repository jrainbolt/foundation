#include "foundation/construction.h"

bool factory_entity_construction_cost(
    FactoryEntityType entity_type,
    FactoryConstructionMaterial *out_cost
)
{
    FactoryConstructionMaterial cost;

    if (out_cost == NULL) {
        return false;
    }
    switch (entity_type) {
        case FACTORY_ENTITY_TYPE_EXTRACTOR:
            cost = FACTORY_CONSTRUCTION_COST_EXTRACTOR;
            break;
        case FACTORY_ENTITY_TYPE_BELT:
            cost = FACTORY_CONSTRUCTION_COST_BELT;
            break;
        case FACTORY_ENTITY_TYPE_STORAGE:
            cost = FACTORY_CONSTRUCTION_COST_STORAGE;
            break;
        case FACTORY_ENTITY_TYPE_REFINERY:
            cost = FACTORY_CONSTRUCTION_COST_REFINERY;
            break;
        case FACTORY_ENTITY_TYPE_ASSEMBLER:
            cost = FACTORY_CONSTRUCTION_COST_ASSEMBLER;
            break;
        case FACTORY_ENTITY_TYPE_SPLITTER:
            cost = FACTORY_CONSTRUCTION_COST_SPLITTER;
            break;
        case FACTORY_ENTITY_TYPE_INSERTER:
            cost = FACTORY_CONSTRUCTION_COST_INSERTER;
            break;
        case FACTORY_ENTITY_TYPE_POWER_POLE:
            cost = FACTORY_CONSTRUCTION_COST_POWER_POLE;
            break;
        case FACTORY_ENTITY_TYPE_POWER_GENERATOR:
            cost = FACTORY_CONSTRUCTION_COST_POWER_GENERATOR;
            break;
        case FACTORY_ENTITY_TYPE_FLUID_TANK:
            cost = FACTORY_CONSTRUCTION_COST_FLUID_TANK;
            break;
        case FACTORY_ENTITY_TYPE_PIPE:
            cost = FACTORY_CONSTRUCTION_COST_PIPE;
            break;
        case FACTORY_ENTITY_TYPE_NONE:
        default:
            return false;
    }
    *out_cost = cost;
    return true;
}
