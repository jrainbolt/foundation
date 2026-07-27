#ifndef FOUNDATION_SNAPSHOT_H
#define FOUNDATION_SNAPSHOT_H

#include <stddef.h>
#include <stdint.h>

#include "foundation/simulation.h"

#define FACTORY_SNAPSHOT_VERSION 15U

typedef struct {
    uint8_t *data;
    size_t size;
} FactorySnapshotBuffer;

FactoryResult factory_simulation_snapshot_size(
    const FactorySimulation *simulation,
    size_t *out_size
);
FactoryResult factory_simulation_save_snapshot(
    const FactorySimulation *simulation,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *out_written
);
FactoryResult factory_simulation_create_snapshot(
    const FactorySimulation *simulation,
    FactorySnapshotBuffer *out_snapshot
);
void factory_snapshot_buffer_destroy(FactorySnapshotBuffer *snapshot);
FactoryResult factory_simulation_load_snapshot(
    const uint8_t *buffer,
    size_t buffer_size,
    FactorySimulation **out_simulation
);

#endif
