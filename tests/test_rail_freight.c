#include <foundation/foundation.h>
#include <foundation/snapshot.h>
#include "logistics_endpoint_internal.h"
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", \
    __FILE__, __LINE__, #x); ++failures; } } while (0)

static FactoryEntityId result_id(FactorySimulation *simulation)
{
    const FactoryCommandResult *result =
        factory_simulation_get_command_result(simulation, 0U);
    CHECK(result != NULL && result->result == FACTORY_RESULT_OK);
    return result != NULL ? result->entity_id : 0U;
}

static FactoryEntityId place_rail(FactorySimulation *simulation, int32_t x)
{
    FactoryCommand command = { FACTORY_COMMAND_PLACE_RAIL,
        { .place_rail = { x, 2, FACTORY_RAIL_HORIZONTAL } } };
    CHECK(factory_simulation_submit_command(simulation, &command) ==
        FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    return result_id(simulation);
}

static FactoryEntityId place_station(FactorySimulation *simulation, int32_t x)
{
    FactoryCommand command = { FACTORY_COMMAND_PLACE_RAIL_STATION,
        { .place_rail_station = { x, 1, FACTORY_DIRECTION_SOUTH } } };
    CHECK(factory_simulation_submit_command(simulation, &command) ==
        FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    return result_id(simulation);
}

static void configure(FactorySimulation *simulation, FactoryEntityId station,
    FactoryItemType item, FactoryRailStationFreightMode mode)
{
    FactoryCommand item_command = {
        FACTORY_COMMAND_SET_RAIL_STATION_FREIGHT_ITEM,
        { .set_rail_station_freight_item = { station, item } } };
    FactoryCommand mode_command = {
        FACTORY_COMMAND_SET_RAIL_STATION_FREIGHT_MODE,
        { .set_rail_station_freight_mode = { station, mode } } };
    CHECK(factory_simulation_submit_command(simulation, &item_command) ==
        FACTORY_RESULT_OK);
    CHECK(factory_simulation_submit_command(simulation, &mode_command) ==
        FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(simulation, 0U)->result ==
        FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(simulation, 1U)->result ==
        FACTORY_RESULT_OK);
}

static void freight_arrival_transfer_snapshot_and_reconfigure(void)
{
    FactoryWorld *world = factory_world_create(12, 5);
    FactorySimulation *simulation =
        factory_simulation_create_with_construction_units(world, 1000U);
    FactoryEntityId rails[8];
    for (int32_t x = 1; x <= 8; ++x) rails[x - 1] = place_rail(simulation, x);
    FactoryEntityId station = place_station(simulation, 8);
    configure(simulation, station, FACTORY_ITEM_IRON_PLATE,
        FACTORY_RAIL_STATION_FREIGHT_UNLOAD);

    FactoryCommand wagon_command = { FACTORY_COMMAND_PLACE_CARGO_WAGON,
        { .place_cargo_wagon = { rails[1], FACTORY_DIRECTION_EAST } } };
    FactoryCommand locomotive_command = { FACTORY_COMMAND_PLACE_LOCOMOTIVE,
        { .place_locomotive = { rails[2], FACTORY_DIRECTION_EAST } } };
    CHECK(factory_simulation_submit_command(simulation, &wagon_command) == 0);
    CHECK(factory_simulation_submit_command(simulation, &locomotive_command) == 0);
    CHECK(factory_simulation_tick(simulation) == 0);
    FactoryEntityId wagon = factory_simulation_get_command_result(simulation, 0U)->entity_id;
    FactoryEntityId train = factory_simulation_get_command_result(simulation, 1U)->entity_id;
    FactoryCommand couple = { FACTORY_COMMAND_COUPLE_REAR_WAGON,
        { .couple_rear_wagon = { train, wagon } } };
    CHECK(factory_simulation_submit_command(simulation, &couple) == 0);
    CHECK(factory_simulation_tick(simulation) == 0);
    CHECK(factory_simulation_cargo_wagon_insert(simulation, wagon,
        FACTORY_ITEM_IRON_PLATE, 30U) == FACTORY_RESULT_OK);
    FactoryCommand destination = { FACTORY_COMMAND_SET_TRAIN_DESTINATION,
        { .set_train_destination = { train, station } } };
    CHECK(factory_simulation_submit_command(simulation, &destination) == 0);
    CHECK(factory_simulation_tick(simulation) == 0);

    FactoryRailStationInspection station_state;
    FactoryLocomotiveInspection locomotive_state;
    for (uint32_t tick = 0U; tick < 40U; ++tick) {
        CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
        CHECK(factory_simulation_get_locomotive(simulation, train,
            &locomotive_state));
        if (locomotive_state.route_status == FACTORY_TRAIN_ROUTE_ARRIVED) break;
    }
    CHECK(locomotive_state.route_status == FACTORY_TRAIN_ROUTE_ARRIVED);
    CHECK(factory_simulation_get_rail_station(simulation, station,
        &station_state));
    CHECK(station_state.freight_quantity == 10U);
    CHECK(station_state.latest_transfer_quantity == 10U);
    CHECK(station_state.latest_transfer_activity ==
        FACTORY_RAIL_FREIGHT_UNLOADING);
    const FactoryEvent *event = NULL;
    for (size_t index = 0U;
        index < factory_simulation_get_event_count(simulation); ++index) {
        const FactoryEvent *candidate =
            factory_simulation_get_event(simulation, index);
        if (candidate != NULL &&
            candidate->type == FACTORY_EVENT_RAIL_FREIGHT_TRANSFERRED)
            event = candidate;
    }
    CHECK(event != NULL && event->type == FACTORY_EVENT_RAIL_FREIGHT_TRANSFERRED
        && event->entity_id == station && event->related_entity_id == wagon
        && event->item_type == FACTORY_ITEM_IRON_PLATE && event->quantity == 10U
        && event->related_quantity == train);

    /* Reservation-derived state settles on the tick after arrival. */
    CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_rail_station(simulation, station,
        &station_state) && station_state.freight_quantity == 20U);

    FactorySnapshotBuffer snapshot = { 0 };
    FactorySimulation *loaded = NULL;
    CHECK(factory_simulation_create_snapshot(simulation, &snapshot) == 0);
    FactoryResult load_result = factory_simulation_load_snapshot(snapshot.data,
        snapshot.size, &loaded);
    CHECK(load_result == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_rail_station(loaded, station, &station_state)
        && station_state.freight_quantity == 20U
        && station_state.latest_transfer_quantity == 0U);
    CHECK(factory_simulation_tick(simulation) == 0);
    CHECK(factory_simulation_tick(loaded) == 0);
    FactorySnapshotBuffer a = { 0 }, b = { 0 };
    CHECK(factory_simulation_create_snapshot(simulation, &a) == 0);
    CHECK(factory_simulation_create_snapshot(loaded, &b) == 0);
    CHECK(a.size == b.size && memcmp(a.data, b.data, a.size) == 0);

    CHECK(factory_simulation_tick(simulation) == 0);
    CHECK(factory_simulation_get_rail_station(simulation, station,
        &station_state) && station_state.freight_quantity == 30U);
    FactoryCommand invalid_item = {
        FACTORY_COMMAND_SET_RAIL_STATION_FREIGHT_ITEM,
        { .set_rail_station_freight_item = { station,
            FACTORY_ITEM_COPPER_PLATE } } };
    CHECK(factory_simulation_submit_command(simulation, &invalid_item) == 0);
    CHECK(factory_simulation_tick(simulation) == 0);
    CHECK(factory_simulation_get_command_result(simulation, 0U)->result ==
        FACTORY_RESULT_INVALID_STATE);
    CHECK(factory_simulation_get_rail_station(simulation, station,
        &station_state) && station_state.configured_item ==
        FACTORY_ITEM_IRON_PLATE && station_state.freight_quantity == 30U);

    FactoryLogisticsEndpoint endpoint = { station,
        FACTORY_LOGISTICS_SLOT_RAIL_STATION_FREIGHT };
    FactoryItemType endpoint_item = FACTORY_ITEM_NONE;
    CHECK(factory_logistics_endpoint_peek(simulation, endpoint,
        &endpoint_item) == FACTORY_LOGISTICS_RESULT_OK
        && endpoint_item == FACTORY_ITEM_IRON_PLATE);
    CHECK(factory_logistics_endpoint_can_accept(simulation, endpoint,
        FACTORY_ITEM_IRON_PLATE) == FACTORY_LOGISTICS_RESULT_INVALID_SLOT);
    FactoryCommand load_mode = {
        FACTORY_COMMAND_SET_RAIL_STATION_FREIGHT_MODE,
        { .set_rail_station_freight_mode = { station,
            FACTORY_RAIL_STATION_FREIGHT_LOAD } } };
    CHECK(factory_simulation_submit_command(simulation, &load_mode) == 0);
    CHECK(factory_simulation_tick(simulation) == 0);
    CHECK(factory_logistics_endpoint_can_accept(simulation, endpoint,
        FACTORY_ITEM_IRON_PLATE) == FACTORY_LOGISTICS_RESULT_OK);
    CHECK(factory_logistics_endpoint_peek(simulation, endpoint,
        &endpoint_item) == FACTORY_LOGISTICS_RESULT_INVALID_SLOT);

    factory_snapshot_buffer_destroy(&a);
    factory_snapshot_buffer_destroy(&b);
    factory_snapshot_buffer_destroy(&snapshot);
    factory_simulation_destroy(loaded);
    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

int main(void)
{
    freight_arrival_transfer_snapshot_and_reconfigure();
    return failures != 0 ? 1 : 0;
}
