#ifndef FOUNDATION_CONSTRUCTION_H
#define FOUNDATION_CONSTRUCTION_H

#include <stdbool.h>
#include <stdint.h>

#include "foundation/entity.h"

#define FACTORY_CONSTRUCTION_COST_EXTRACTOR 10U
#define FACTORY_CONSTRUCTION_COST_BELT 1U
#define FACTORY_CONSTRUCTION_COST_STORAGE 8U
#define FACTORY_CONSTRUCTION_COST_REFINERY 15U
#define FACTORY_CONSTRUCTION_COST_ASSEMBLER 20U
#define FACTORY_CONSTRUCTION_COST_SPLITTER 5U
#define FACTORY_CONSTRUCTION_COST_INSERTER 4U

typedef uint32_t FactoryConstructionMaterial;

bool factory_entity_construction_cost(
    FactoryEntityType entity_type,
    FactoryConstructionMaterial *out_cost
);

#endif
