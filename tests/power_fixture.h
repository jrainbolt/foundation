#ifndef FOUNDATION_TEST_POWER_FIXTURE_H
#define FOUNDATION_TEST_POWER_FIXTURE_H

#include "foundation/simulation.h"

static inline bool factory_test_submit_power_pair(
    FactorySimulation *simulation,
    int32_t pole_x,
    int32_t pole_y,
    int32_t generator_x,
    int32_t generator_y
)
{
    FactoryCommand pole = {
        FACTORY_COMMAND_PLACE_POWER_POLE,
        {.place_power_pole = {pole_x, pole_y}}
    };
    FactoryCommand generator = {
        FACTORY_COMMAND_PLACE_POWER_GENERATOR,
        {.place_power_generator = {generator_x, generator_y}}
    };
    return factory_simulation_submit_command(simulation, &pole)
            == FACTORY_RESULT_OK
        && factory_simulation_submit_command(simulation, &generator)
            == FACTORY_RESULT_OK;
}

/*
 * Submit real power infrastructure after a fixture's gameplay placement
 * commands. The caller reserves two otherwise-unused rows beginning at
 * gameplay_height. Gameplay entity IDs and command-result indices therefore
 * remain stable.
 */
static inline bool factory_test_submit_power_row(
    FactorySimulation *simulation,
    uint32_t width,
    uint32_t gameplay_height
)
{
    uint32_t pole_x;
    uint32_t last_pole = 0U;
    if (simulation == NULL || width == 0U || gameplay_height > INT32_MAX - 1)
        return false;
    for (pole_x = 2U; pole_x < width; pole_x += 6U) {
        FactoryCommand pole = {
            FACTORY_COMMAND_PLACE_POWER_POLE,
            {.place_power_pole = {
                (int32_t)pole_x, (int32_t)gameplay_height
            }}
        };
        FactoryCommand generator = {
            FACTORY_COMMAND_PLACE_POWER_GENERATOR,
            {.place_power_generator = {
                (int32_t)pole_x, (int32_t)gameplay_height + 1
            }}
        };
        if (factory_simulation_submit_command(simulation, &pole)
                != FACTORY_RESULT_OK
            || factory_simulation_submit_command(simulation, &generator)
                != FACTORY_RESULT_OK)
            return false;
        last_pole = pole_x;
    }
    if (width > 2U && width - 1U > last_pole + 3U) {
        FactoryCommand pole = {
            FACTORY_COMMAND_PLACE_POWER_POLE,
            {.place_power_pole = {
                (int32_t)(width - 1U), (int32_t)gameplay_height
            }}
        };
        FactoryCommand generator = {
            FACTORY_COMMAND_PLACE_POWER_GENERATOR,
            {.place_power_generator = {
                (int32_t)(width - 1U), (int32_t)gameplay_height + 1
            }}
        };
        if (factory_simulation_submit_command(simulation, &pole)
                != FACTORY_RESULT_OK
            || factory_simulation_submit_command(simulation, &generator)
                != FACTORY_RESULT_OK)
            return false;
    }
    if (width <= 2U) {
        FactoryCommand pole = {
            FACTORY_COMMAND_PLACE_POWER_POLE,
            {.place_power_pole = {0, (int32_t)gameplay_height}}
        };
        FactoryCommand generator = {
            FACTORY_COMMAND_PLACE_POWER_GENERATOR,
            {.place_power_generator = {0, (int32_t)gameplay_height + 1}}
        };
        if (factory_simulation_submit_command(simulation, &pole)
                != FACTORY_RESULT_OK
            || factory_simulation_submit_command(simulation, &generator)
                != FACTORY_RESULT_OK)
            return false;
    }
    return true;
}

#endif
