#ifndef FOUNDATION_SOLAR_H
#define FOUNDATION_SOLAR_H

#include <stdbool.h>
#include <stdint.h>

#include "foundation/power.h"

#define FACTORY_SOLAR_GENERATOR_MAX_OUTPUT 100U

typedef struct {
    FactoryEntityId entity_id;
    FactoryPowerUnits maximum_output;
    FactoryPowerUnits available_output;
    FactoryPowerUnits generated_last_tick;
    bool active;
} FactorySolarGeneratorInspection;

typedef struct FactorySimulation FactorySimulation;

FactoryResult factory_simulation_get_solar_generator(
    const FactorySimulation *simulation,
    FactoryEntityId entity_id,
    FactorySolarGeneratorInspection *out_generator);

#endif
