#ifndef FOUNDATION_SIMULATION_INTERNAL_H
#define FOUNDATION_SIMULATION_INTERNAL_H

#include "foundation/simulation.h"

#include "assembler_internal.h"
#include "belt_internal.h"
#include "extractor_internal.h"
#include "inserter_internal.h"
#include "refinery_internal.h"
#include "splitter_internal.h"
#include "storage_internal.h"
#include "construction_inventory_internal.h"

struct FactorySimulation {
    uint64_t tick;
    FactoryWorld *world;
    FactoryEntityManager *entities;
    FactoryExtractorStore extractors;
    FactoryRefineryStore refineries;
    FactoryAssemblerStore assemblers;
    FactorySplitterStore splitters;
    FactoryInserterStore inserters;
    FactoryBeltStore belts;
    FactoryStorageStore storages;
    FactoryConstructionInventory construction_inventory;
    FactoryCommand commands[FACTORY_COMMAND_QUEUE_CAPACITY];
    size_t command_count;
    FactoryCommandResult results[FACTORY_COMMAND_QUEUE_CAPACITY];
    size_t result_count;
};

#endif
