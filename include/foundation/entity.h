#ifndef FOUNDATION_ENTITY_H
#define FOUNDATION_ENTITY_H

#include <stdbool.h>
#include <stdint.h>

typedef uint32_t FactoryEntityId;
typedef struct FactoryEntityManager FactoryEntityManager;

/*
 * Creates an empty entity manager. The caller owns the returned manager and
 * must destroy it with factory_entity_manager_destroy.
 */
FactoryEntityManager *factory_entity_manager_create(void);

/* Destroys a manager. Passing NULL is safe. */
void factory_entity_manager_destroy(FactoryEntityManager *manager);

/*
 * Creates an entity and returns its nonzero ID. Returns zero when manager is
 * NULL, memory allocation fails, or the ID space is exhausted.
 */
FactoryEntityId factory_entity_create(FactoryEntityManager *manager);

/* Returns true only while id names a live entity in manager. */
bool factory_entity_is_valid(
    const FactoryEntityManager *manager,
    FactoryEntityId id
);

/* Invalidates a live entity. NULL managers and invalid IDs are ignored. */
void factory_entity_destroy(
    FactoryEntityManager *manager,
    FactoryEntityId id
);

#endif
