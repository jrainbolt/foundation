#include "foundation/construction.h"
#include "foundation/content.h"

bool factory_entity_construction_cost(
    FactoryEntityType entity_type,
    FactoryConstructionMaterial *out_cost
)
{
    const FactoryEntityDefinition *definition=
        factory_content_entity_definition_get(entity_type);
    if (out_cost==NULL || definition==NULL) return false;
    *out_cost=definition->construction_cost;
    return true;
}
