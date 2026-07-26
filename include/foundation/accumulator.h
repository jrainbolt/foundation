#ifndef FOUNDATION_ACCUMULATOR_H
#define FOUNDATION_ACCUMULATOR_H

#include <stdbool.h>
#include <stdint.h>

#include "foundation/power.h"

typedef uint64_t FactoryElectricalEnergy;

#define FACTORY_ACCUMULATOR_CAPACITY UINT64_C(10000)
#define FACTORY_ACCUMULATOR_MAX_CHARGE_RATE 100U
#define FACTORY_ACCUMULATOR_MAX_DISCHARGE_RATE 100U

typedef enum {
    FACTORY_ACCUMULATOR_IDLE = 0,
    FACTORY_ACCUMULATOR_CHARGING,
    FACTORY_ACCUMULATOR_DISCHARGING
} FactoryAccumulatorActivity;

typedef struct {
    FactoryEntityId entity_id;
    FactoryElectricalEnergy stored_energy;
    FactoryElectricalEnergy capacity;
    FactoryPowerUnits maximum_charge_rate;
    FactoryPowerUnits maximum_discharge_rate;
    FactoryPowerUnits charged_last_tick;
    FactoryPowerUnits discharged_last_tick;
    FactoryAccumulatorActivity activity;
    FactoryEntityId attached_pole_id;
    FactoryPowerNetworkId network_id;
    bool connected;
} FactoryAccumulatorInspection;

typedef struct FactorySimulation FactorySimulation;

/*
 * Reads authoritative stored energy plus the latest completed tick's transient
 * activity and derived power-network attachment. Capacity and rates are fixed
 * definition values. Returns FACTORY_RESULT_ENTITY_NOT_FOUND when entity_id
 * does not identify an accumulator.
 */
FactoryResult factory_simulation_get_accumulator(
    const FactorySimulation *simulation,
    FactoryEntityId entity_id,
    FactoryAccumulatorInspection *out_accumulator);

#endif
