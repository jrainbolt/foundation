#ifndef FOUNDATION_TELEMETRY_H
#define FOUNDATION_TELEMETRY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "foundation/presentation.h"

#define FACTORY_TELEMETRY_COUNTER_MAX UINT64_MAX
#define FACTORY_TELEMETRY_DEFAULT_MAX_ENTITIES 256U

typedef enum {
    FACTORY_TELEMETRY_RESULT_OK = 0,
    FACTORY_TELEMETRY_RESULT_DUPLICATE,
    FACTORY_TELEMETRY_RESULT_NON_SEQUENTIAL,
    FACTORY_TELEMETRY_RESULT_INVALID_ARGUMENT,
    FACTORY_TELEMETRY_RESULT_OUT_OF_MEMORY,
    FACTORY_TELEMETRY_RESULT_ENTITY_CAPACITY
} FactoryTelemetryResult;

typedef enum {
    FACTORY_TELEMETRY_WINDOW_CURRENT_TICK = 0,
    FACTORY_TELEMETRY_WINDOW_SHORT,
    FACTORY_TELEMETRY_WINDOW_LONG
} FactoryTelemetryWindow;

typedef struct {
    uint32_t short_window_ticks;
    uint32_t long_window_ticks;
    uint32_t maximum_entities;
} FactoryTelemetryConfig;

typedef struct {
    uint64_t extracted_quantity;
    uint64_t produced_quantity;
    uint64_t transferred_quantity;
    uint64_t storage_inflow;
    uint64_t storage_outflow;
    uint32_t observed_ticks;
    bool saturated;
} FactoryTelemetryItemMetrics;

typedef struct {
    uint64_t received_quantity;
    uint64_t sent_quantity;
    uint64_t produced_quantity;
    int64_t net_flow;
    uint32_t observed_ticks;
    bool saturated;
} FactoryTelemetryEntityItemMetrics;

typedef struct {
    FactoryEntityId entity_id;
    FactoryEntityType entity_type;
    uint64_t received_quantity;
    uint64_t sent_quantity;
    int64_t net_flow;
    uint64_t completed_cycles;
    uint32_t observed_ticks;
    uint32_t working_ticks;
    uint32_t blocked_input_ticks;
    uint32_t blocked_output_ticks;
    uint32_t unpowered_ticks;
    uint32_t idle_ticks;
    uint32_t depleted_ticks;
    uint32_t occupied_ticks;
    uint32_t empty_ticks;
    uint32_t blocked_ticks;
    uint32_t holding_ticks;
    uint64_t pickups;
    uint64_t drops;
    bool saturated;
} FactoryTelemetryEntityMetrics;

typedef struct FactoryTelemetry FactoryTelemetry;

/* A collector is externally owned and never becomes part of simulation state.
 * It allocates its bounded history at creation and retains no simulation
 * pointer after observation. */
void factory_telemetry_default_config(FactoryTelemetryConfig *out_config);
FactoryTelemetry *factory_telemetry_create(
    const FactoryTelemetryConfig *config);
void factory_telemetry_destroy(FactoryTelemetry *telemetry);
/* Clears history and sequence identity. The next completed nonzero tick may be
 * attached as a new first observation. */
void factory_telemetry_clear(FactoryTelemetry *telemetry);
/* Observe once immediately after a successful simulation tick, while that
 * tick's event batch is visible. Duplicate observation is a no-op; after the
 * first observation, skipped or earlier ticks are rejected without mutation. */
FactoryTelemetryResult factory_telemetry_observe_step(
    FactoryTelemetry *telemetry,const FactorySimulation *simulation);
/* Queries copy integer totals for the requested fixed tick window. They return
 * false, with a zero result, when no applicable observation exists. Net flow
 * is received minus sent and saturates at INT64_MIN/INT64_MAX. */
bool factory_telemetry_get_item_metrics(
    const FactoryTelemetry *telemetry,FactoryItemType item,
    FactoryTelemetryWindow window,FactoryTelemetryItemMetrics *out_metrics);
bool factory_telemetry_get_entity_metrics(
    const FactoryTelemetry *telemetry,FactoryEntityId entity_id,
    FactoryTelemetryWindow window,FactoryTelemetryEntityMetrics *out_metrics);
bool factory_telemetry_get_entity_item_metrics(
    const FactoryTelemetry *telemetry,FactoryEntityId entity_id,
    FactoryItemType item,FactoryTelemetryWindow window,
    FactoryTelemetryEntityItemMetrics *out_metrics);
uint64_t factory_telemetry_get_last_observed_tick(
    const FactoryTelemetry *telemetry);
const FactoryTelemetryConfig *factory_telemetry_get_config(
    const FactoryTelemetry *telemetry);

#endif
