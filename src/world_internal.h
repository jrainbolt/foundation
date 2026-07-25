#ifndef FOUNDATION_WORLD_INTERNAL_H
#define FOUNDATION_WORLD_INTERNAL_H

#include <foundation/world.h>

FactoryResult factory_world_set_occupying_entity(
    FactoryWorld *world,
    int32_t x,
    int32_t y,
    FactoryEntityId entity_id
);

FactoryResult factory_world_consume_resource(
    FactoryWorld *world,
    int32_t x,
    int32_t y,
    uint32_t amount
);

FactoryResult factory_world_clear_occupying_entity(
    FactoryWorld *world,
    int32_t x,
    int32_t y,
    FactoryEntityId expected_entity_id
);

#endif
