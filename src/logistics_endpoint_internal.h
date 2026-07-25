#ifndef FOUNDATION_LOGISTICS_ENDPOINT_INTERNAL_H
#define FOUNDATION_LOGISTICS_ENDPOINT_INTERNAL_H

#include "foundation/simulation.h"

typedef enum {
    FACTORY_LOGISTICS_SLOT_NONE = 0,
    FACTORY_LOGISTICS_SLOT_MAIN,
    FACTORY_LOGISTICS_SLOT_INPUT,
    FACTORY_LOGISTICS_SLOT_OUTPUT,
    FACTORY_LOGISTICS_SLOT_ASSEMBLER_INPUT_0,
    FACTORY_LOGISTICS_SLOT_ASSEMBLER_INPUT_1,
    FACTORY_LOGISTICS_SLOT_SPLITTER_INPUT,
    FACTORY_LOGISTICS_SLOT_SPLITTER_LEFT_OUTPUT,
    FACTORY_LOGISTICS_SLOT_SPLITTER_RIGHT_OUTPUT,
    FACTORY_LOGISTICS_SLOT_INSERTER_HELD,
    FACTORY_LOGISTICS_SLOT_STORAGE_INPUT
} FactoryLogisticsSlot;

typedef struct {
    FactoryEntityId entity_id;
    FactoryLogisticsSlot slot;
} FactoryLogisticsEndpoint;

typedef enum {
    FACTORY_LOGISTICS_RESULT_OK = 0,
    FACTORY_LOGISTICS_RESULT_EMPTY,
    FACTORY_LOGISTICS_RESULT_BLOCKED,
    FACTORY_LOGISTICS_RESULT_INVALID_ENTITY,
    FACTORY_LOGISTICS_RESULT_INVALID_SLOT,
    FACTORY_LOGISTICS_RESULT_INVALID_ITEM,
    FACTORY_LOGISTICS_RESULT_INCOMPATIBLE_ITEM,
    FACTORY_LOGISTICS_RESULT_STATE_MISMATCH
} FactoryLogisticsResult;

bool factory_logistics_endpoint_equal(
    FactoryLogisticsEndpoint left,
    FactoryLogisticsEndpoint right
);
FactoryLogisticsResult factory_logistics_endpoint_peek(
    const FactorySimulation *simulation,
    FactoryLogisticsEndpoint endpoint,
    FactoryItemType *out_item
);
FactoryLogisticsResult factory_logistics_endpoint_can_accept(
    const FactorySimulation *simulation,
    FactoryLogisticsEndpoint endpoint,
    FactoryItemType item
);
FactoryLogisticsResult factory_logistics_endpoint_remove(
    FactorySimulation *simulation,
    FactoryLogisticsEndpoint endpoint,
    FactoryItemType expected_item
);
FactoryLogisticsResult factory_logistics_endpoint_insert(
    FactorySimulation *simulation,
    FactoryLogisticsEndpoint endpoint,
    FactoryItemType item
);
FactoryLogisticsResult factory_logistics_endpoint_transfer(
    FactorySimulation *simulation,
    FactoryLogisticsEndpoint source,
    FactoryLogisticsEndpoint destination,
    FactoryItemType expected_item
);

#endif
