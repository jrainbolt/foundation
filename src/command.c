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
        default:
            return false;
    }
    return direction >= FACTORY_DIRECTION_NORTH
        && direction <= FACTORY_DIRECTION_WEST;
}
