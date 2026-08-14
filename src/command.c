#include "foundation/command.h"
#include "foundation/rail.h"

#include <stddef.h>

bool factory_command_is_well_formed(const FactoryCommand *command)
{
    FactoryDirection direction;

    if (command == NULL) {
        return false;
    }
    switch (command->type) {
        case FACTORY_COMMAND_PLACE_EXTRACTOR:
            direction = command->data.place_extractor.output_direction;
            break;
        case FACTORY_COMMAND_PLACE_BELT:
            direction = command->data.place_belt.direction;
            break;
        case FACTORY_COMMAND_PLACE_STORAGE:
            return true;
        case FACTORY_COMMAND_PLACE_REFINERY:
            direction = command->data.place_refinery.input_direction;
            if (direction < FACTORY_DIRECTION_NORTH
                || direction > FACTORY_DIRECTION_WEST) {
                return false;
            }
            direction = command->data.place_refinery.output_direction;
            return direction >= FACTORY_DIRECTION_NORTH
                && direction <= FACTORY_DIRECTION_WEST
                && command->data.place_refinery.input_direction != direction;
        case FACTORY_COMMAND_SET_REFINERY_RECIPE:
            return true;
        case FACTORY_COMMAND_PLACE_ASSEMBLER:
            direction = command->data.place_assembler.output_direction;
            break;
        case FACTORY_COMMAND_DEMOLISH_ENTITY:
            return true;
        case FACTORY_COMMAND_PLACE_SPLITTER:
            direction = command->data.place_splitter.facing;
            break;
        case FACTORY_COMMAND_PLACE_INSERTER:
            direction = command->data.place_inserter.facing;
            break;
        case FACTORY_COMMAND_GRANT_CONSTRUCTION_UNITS:
            return true;
        case FACTORY_COMMAND_SET_ASSEMBLER_RECIPE:
            return command->data.set_assembler_recipe.recipe_id
                >= FACTORY_ASSEMBLER_RECIPE_NONE
                && command->data.set_assembler_recipe.recipe_id
                    < FACTORY_ASSEMBLER_RECIPE_COUNT;
        case FACTORY_COMMAND_SET_STORAGE_OUTPUT:
            return command->data.set_storage_output.item
                >= FACTORY_ITEM_NONE
                && command->data.set_storage_output.item
                    <= FACTORY_ITEM_CONSTRUCTION_MATERIAL;
        case FACTORY_COMMAND_PLACE_POWER_POLE:
        case FACTORY_COMMAND_PLACE_POWER_GENERATOR:
        case FACTORY_COMMAND_PLACE_FLUID_TANK:
        case FACTORY_COMMAND_PLACE_PIPE:
        case FACTORY_COMMAND_PLACE_WATER_EXTRACTOR:
        case FACTORY_COMMAND_PLACE_BOILER:
        case FACTORY_COMMAND_PLACE_STEAM_ENGINE:
        case FACTORY_COMMAND_PLACE_SOLAR_GENERATOR:
        case FACTORY_COMMAND_PLACE_ACCUMULATOR:
        case FACTORY_COMMAND_PLACE_REACTOR_CORE:
        case FACTORY_COMMAND_PLACE_HEAT_CONDUCTOR:
        case FACTORY_COMMAND_PLACE_HEAT_EXCHANGER:
        case FACTORY_COMMAND_PLACE_STEAM_TURBINE:
        case FACTORY_COMMAND_PLACE_STEAM_CONDENSER:
        case FACTORY_COMMAND_PLACE_RESEARCH_LAB:
        case FACTORY_COMMAND_PLACE_CONSTRUCTION_DEPOT:
            return true;
        case FACTORY_COMMAND_PLACE_RAIL:
            return factory_rail_geometry_is_valid(
                (FactoryRailGeometry)command->data.place_rail.geometry);
        case FACTORY_COMMAND_PLACE_RAIL_STATION:
            direction=command->data.place_rail_station.orientation;
            break;
        case FACTORY_COMMAND_PLACE_RAIL_SIGNAL:
            direction=command->data.place_rail_signal.orientation;
            break;
        case FACTORY_COMMAND_PLACE_RAIL_CHAIN_SIGNAL:
            direction=command->data.place_rail_chain_signal.orientation;
            break;
        case FACTORY_COMMAND_SET_RAIL_STATION_FREIGHT_MODE:
            return command->data.set_rail_station_freight_mode.station_entity_id!=0U
                &&command->data.set_rail_station_freight_mode.mode
                    <=FACTORY_RAIL_STATION_FREIGHT_UNLOAD;
        case FACTORY_COMMAND_SET_RAIL_STATION_FREIGHT_ITEM:
            return command->data.set_rail_station_freight_item.station_entity_id!=0U
                &&command->data.set_rail_station_freight_item.item>=FACTORY_ITEM_NONE
                &&command->data.set_rail_station_freight_item.item
                    <=FACTORY_ITEM_CONSTRUCTION_MATERIAL;
        case FACTORY_COMMAND_PLACE_RAIL_SWITCH:
            return factory_rail_switch_geometry_is_valid(
                (FactoryRailSwitchGeometry)
                    command->data.place_rail_switch.geometry);
        case FACTORY_COMMAND_SET_RAIL_SWITCH_BRANCH:
            return command->data.set_rail_switch_branch.entity_id!=0U
                &&factory_rail_switch_branch_is_valid(
                    (FactoryRailSwitchBranch)
                        command->data.set_rail_switch_branch.branch);
        case FACTORY_COMMAND_PLACE_LOCOMOTIVE:
            direction=command->data.place_locomotive.direction;
            return command->data.place_locomotive.rail_entity_id!=0U
                &&direction>=FACTORY_DIRECTION_NORTH
                &&direction<=FACTORY_DIRECTION_WEST;
        case FACTORY_COMMAND_PLACE_CARGO_WAGON:
            direction=command->data.place_cargo_wagon.direction;
            return command->data.place_cargo_wagon.rail_entity_id!=0U
                &&direction>=FACTORY_DIRECTION_NORTH
                &&direction<=FACTORY_DIRECTION_WEST;
        case FACTORY_COMMAND_COUPLE_REAR_WAGON:
            return command->data.couple_rear_wagon.locomotive_entity_id!=0U
                &&command->data.couple_rear_wagon.wagon_entity_id!=0U;
        case FACTORY_COMMAND_DECOUPLE_REAR_WAGON:
            return command->data.decouple_rear_wagon.locomotive_entity_id!=0U;
        case FACTORY_COMMAND_SET_TRAIN_DESTINATION:
            return command->data.set_train_destination.train_id!=0U
                &&command->data.set_train_destination.station_entity_id!=0U;
        case FACTORY_COMMAND_CLEAR_TRAIN_DESTINATION:
            return command->data.clear_train_destination.train_id!=0U;
        case FACTORY_COMMAND_REPLAN_TRAIN_ROUTE:
            return command->data.replan_train_route.train_id!=0U;
        case FACTORY_COMMAND_TRAIN_SCHEDULE_ADD_STOP:
            return command->data.train_schedule_add_stop.train_id!=0U
                &&command->data.train_schedule_add_stop.station_entity_id!=0U
                &&command->data.train_schedule_add_stop.wait_condition
                    <=FACTORY_TRAIN_WAIT_CARGO_FULL
                &&((command->data.train_schedule_add_stop.wait_condition
                        ==FACTORY_TRAIN_WAIT_TIME
                    &&command->data.train_schedule_add_stop.wait_value!=0U
                    &&command->data.train_schedule_add_stop.wait_value
                        <=FACTORY_TRAIN_WAIT_TIME_MAX)
                   ||(command->data.train_schedule_add_stop.wait_condition
                        !=FACTORY_TRAIN_WAIT_TIME
                    &&command->data.train_schedule_add_stop.wait_value==0U));
        case FACTORY_COMMAND_TRAIN_SCHEDULE_REMOVE_STOP:
            return command->data.train_schedule_remove_stop.train_id!=0U;
        case FACTORY_COMMAND_TRAIN_SCHEDULE_CLEAR:
            return command->data.train_schedule_clear.train_id!=0U;
        case FACTORY_COMMAND_TRAIN_SCHEDULE_SET_ENABLED:
            return command->data.train_schedule_set_enabled.train_id!=0U;
        case FACTORY_COMMAND_INSERT_REACTOR_FUEL:
            return command->data.insert_reactor_fuel.reactor_entity_id != 0U
                && factory_nuclear_fuel_definition_get(
                    command->data.insert_reactor_fuel.fuel_id) != NULL;
        case FACTORY_COMMAND_SELECT_RESEARCH:
            return command->data.select_research.technology_id
                != FACTORY_TECHNOLOGY_NONE;
        case FACTORY_COMMAND_FLUID_INSERT:
            return command->data.fluid_insert.destination_entity_id != 0U
                && factory_fluid_definition_get(
                    command->data.fluid_insert.fluid_type) != NULL
                && command->data.fluid_insert.quantity != 0U;
        case FACTORY_COMMAND_FLUID_REMOVE:
            return command->data.fluid_remove.source_entity_id != 0U
                && command->data.fluid_remove.quantity != 0U;
        case FACTORY_COMMAND_FLUID_TRANSFER:
            return command->data.fluid_transfer.source_entity_id != 0U
                && command->data.fluid_transfer.destination_entity_id != 0U
                && command->data.fluid_transfer.source_entity_id
                    != command->data.fluid_transfer.destination_entity_id
                && command->data.fluid_transfer.quantity != 0U;
        default:
            return false;
    }
    return direction >= FACTORY_DIRECTION_NORTH
        && direction <= FACTORY_DIRECTION_WEST;
}
