#ifndef FOUNDATION_TICK_PREFLIGHT_INTERNAL_H
#define FOUNDATION_TICK_PREFLIGHT_INTERNAL_H

#include "foundation/simulation.h"

typedef enum {
    FACTORY_TOPOLOGY_POWER = 0,
    FACTORY_TOPOLOGY_FLUID,
    FACTORY_TOPOLOGY_HEAT,
    FACTORY_TOPOLOGY_DOMAIN_COUNT
} FactoryTopologyDomain;

#define FACTORY_TOPOLOGY_BLOCK_COUNT 16U

typedef struct {
    void *pointer;
    size_t bytes;
    size_t width;
    FactoryTopologyDomain domain;
} FactoryTopologyBlock;

typedef struct {
    FactoryTopologyBlock blocks[FACTORY_TOPOLOGY_BLOCK_COUNT];
    size_t count;
    size_t cursor[FACTORY_TOPOLOGY_DOMAIN_COUNT];
    bool active;
} FactoryTickPreflight;

FactoryResult factory_simulation_preflight_tick(FactorySimulation *simulation);
void *factory_topology_calloc(
    FactorySimulation *simulation, FactoryTopologyDomain domain,
    size_t count, size_t width);
void factory_tick_preflight_finish(FactorySimulation *simulation);
void factory_tick_preflight_destroy(FactoryTickPreflight *preflight);

/* Test-only deterministic allocation-failure seam; SIZE_MAX disables it. */
void factory_tick_preflight_test_fail_allocations_after(size_t successes);

#endif
