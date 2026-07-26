#ifndef FOUNDATION_ENTITY_INTERNAL_H
#define FOUNDATION_ENTITY_INTERNAL_H

#include "foundation/entity.h"

struct FactoryEntityManager {
    FactoryEntityId next_id;
    FactoryEntityId *live_ids;
    size_t count;
    size_t capacity;
};

#endif
