#ifndef FOUNDATION_CLOCK_INTERNAL_H
#define FOUNDATION_CLOCK_INTERNAL_H

#include "foundation/clock.h"
#include <stdbool.h>

void factory_simulation_clock_set(
    FactorySimulationClock *clock, uint64_t tick);
bool factory_simulation_clock_can_advance(
    const FactorySimulationClock *clock);
void factory_simulation_clock_advance(
    FactorySimulationClock *clock);

#endif
