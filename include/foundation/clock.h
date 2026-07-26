#ifndef FOUNDATION_CLOCK_H
#define FOUNDATION_CLOCK_H

#include <stdint.h>

#define FACTORY_CLOCK_TICKS_PER_DAY 2400U
#define FACTORY_CLOCK_SUNRISE 600U
#define FACTORY_CLOCK_FULL_DAYLIGHT_START 900U
#define FACTORY_CLOCK_FULL_DAYLIGHT_END 1500U
#define FACTORY_CLOCK_SUNSET 1800U
#define FACTORY_SOLAR_INTENSITY_SCALE 1000U

typedef struct {
    uint64_t tick;
    uint64_t day;
    uint32_t time_of_day;
} FactorySimulationClock;

typedef struct FactorySimulation FactorySimulation;

uint64_t factory_simulation_clock_get_tick(
    const FactorySimulation *simulation);
uint64_t factory_simulation_clock_get_day(
    const FactorySimulation *simulation);
uint32_t factory_simulation_clock_get_time_of_day(
    const FactorySimulation *simulation);

/* Returns an integer intensity in [0, FACTORY_SOLAR_INTENSITY_SCALE]. */
uint32_t factory_solar_intensity(uint32_t time_of_day);

#endif
