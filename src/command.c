#include "foundation/command.h"

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
                    <= FACTORY_ITEM_BASIC_SCIENCE;
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
            return true;
        case FACTORY_COMMAND_INSERT_REACTOR_FUEL:
            return command->data.insert_reactor_fuel.reactor_entity_id != 0U
                && factory_nuclear_fuel_definition_get(
                    command->data.insert_reactor_fuel.fuel_id) != NULL;
        case FACTORY_COMMAND_SELECT_RESEARCH:
            return command->data.select_research.technology_id
                != FACTORY_TECHNOLOGY_NONE;
        case FACTORY_COMMAND_INSERT_RESEARCH_SCIENCE:
            return command->data.insert_research_science.quantity!=0U;
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
