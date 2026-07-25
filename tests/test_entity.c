#include "foundation/entity.h"

#include <stdbool.h>
#include <stdio.h>

static int failures = 0;

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            (void)fprintf(stderr, "FAIL %s:%d: %s\n",                         \
                __FILE__, __LINE__, #condition);                               \
            ++failures;                                                        \
        }                                                                      \
    } while (false)

int main(void)
{
    FactoryEntityManager *manager = factory_entity_manager_create();
    FactoryEntityId first = 0U;
    FactoryEntityId second = 0U;
    FactoryEntityId third = 0U;

    CHECK(manager != NULL);
    CHECK(!factory_entity_is_valid(manager, 0U));
    CHECK(factory_entity_create(NULL) == 0U);

    first = factory_entity_create(manager);
    second = factory_entity_create(manager);
    CHECK(first != 0U);
    CHECK(second != 0U);
    CHECK(first != second);
    CHECK(factory_entity_is_valid(manager, first));
    CHECK(factory_entity_is_valid(manager, second));

    factory_entity_destroy(manager, first);
    CHECK(!factory_entity_is_valid(manager, first));
    CHECK(factory_entity_is_valid(manager, second));

    third = factory_entity_create(manager);
    CHECK(third != 0U);
    CHECK(third != first);
    CHECK(third != second);
    CHECK(factory_entity_is_valid(manager, third));

    factory_entity_destroy(manager, 0U);
    factory_entity_destroy(manager, 9999U);
    factory_entity_destroy(NULL, second);
    CHECK(!factory_entity_is_valid(NULL, second));
    CHECK(factory_entity_is_valid(manager, second));

    factory_entity_manager_destroy(manager);
    factory_entity_manager_destroy(NULL);

    if (failures != 0) {
        (void)fprintf(stderr, "%d test assertion(s) failed.\n", failures);
        return 1;
    }
    (void)printf("All entity tests passed.\n");
    return 0;
}
