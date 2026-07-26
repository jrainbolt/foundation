#include "clock_internal.h"

#include "simulation_internal.h"

#include <stdbool.h>

void factory_simulation_clock_set(
    FactorySimulationClock *clock, uint64_t tick)
{
    if (clock == NULL) return;
    clock->tick = tick;
    clock->day = tick / FACTORY_CLOCK_TICKS_PER_DAY;
    clock->time_of_day = (uint32_t)(tick % FACTORY_CLOCK_TICKS_PER_DAY);
}

bool factory_simulation_clock_can_advance(const FactorySimulationClock *clock)
{
    return clock != NULL && clock->tick != UINT64_MAX;
}

void factory_simulation_clock_advance(FactorySimulationClock *clock)
{
    if (!factory_simulation_clock_can_advance(clock)) return;
    factory_simulation_clock_set(clock, clock->tick + 1U);
}

uint64_t factory_simulation_clock_get_tick(
    const FactorySimulation *simulation)
{
    return simulation == NULL ? 0U : simulation->clock.tick;
}

uint64_t factory_simulation_clock_get_day(
    const FactorySimulation *simulation)
{
    return simulation == NULL ? 0U : simulation->clock.day;
}

uint32_t factory_simulation_clock_get_time_of_day(
    const FactorySimulation *simulation)
{
    return simulation == NULL ? 0U : simulation->clock.time_of_day;
}

uint32_t factory_solar_intensity(uint32_t time_of_day)
{
    if (time_of_day >= FACTORY_CLOCK_TICKS_PER_DAY)
        return 0U;
    if (time_of_day < FACTORY_CLOCK_SUNRISE
        || time_of_day >= FACTORY_CLOCK_SUNSET)
        return 0U;
    if (time_of_day < FACTORY_CLOCK_FULL_DAYLIGHT_START)
        return (time_of_day - FACTORY_CLOCK_SUNRISE)
            * FACTORY_SOLAR_INTENSITY_SCALE
            / (FACTORY_CLOCK_FULL_DAYLIGHT_START - FACTORY_CLOCK_SUNRISE);
    if (time_of_day < FACTORY_CLOCK_FULL_DAYLIGHT_END)
        return FACTORY_SOLAR_INTENSITY_SCALE;
    return (FACTORY_CLOCK_SUNSET - time_of_day)
        * FACTORY_SOLAR_INTENSITY_SCALE
        / (FACTORY_CLOCK_SUNSET - FACTORY_CLOCK_FULL_DAYLIGHT_END);
}
