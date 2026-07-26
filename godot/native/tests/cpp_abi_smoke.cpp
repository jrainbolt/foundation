#include <foundation/foundation.h>

int main()
{
    FactoryWorld *world = factory_world_create(2U, 2U);
    if (world == nullptr)
        return 1;
    FactorySimulation *simulation =
        factory_simulation_create_with_construction_units(world, 1U);
    FactoryPresentationSnapshot *presentation =
        factory_presentation_snapshot_create();
    if (simulation == nullptr || presentation == nullptr)
        return 2;
    if (factory_simulation_tick(simulation) != FACTORY_RESULT_OK)
        return 3;
    if (factory_presentation_snapshot_rebuild(presentation, simulation)
        != FACTORY_RESULT_OK)
        return 4;
    if (factory_presentation_snapshot_get_tick(presentation) != 1U)
        return 5;
    factory_presentation_snapshot_destroy(presentation);
    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
    return 0;
}
