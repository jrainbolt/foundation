extends SceneTree

func _require(condition: bool, message: String) -> bool:
	if condition:
		return true
	push_error(message)
	quit(1)
	return false

func _initialize() -> void:
	if OS.has_environment("FOUNDATION_SMOKE_FORCE_FAILURE"):
		_require(false, "forced smoke-test failure")
		return
	if not _require(
			ClassDB.class_exists("FoundationSimulation"),
			"FoundationSimulation GDExtension class is not registered"
		):
		return
	var simulation: Object = ClassDB.instantiate("FoundationSimulation")
	if not _require(simulation != null, "adapter is not constructible"):
		return
	if not _require(simulation.get_tick() == 0, "unsafe initial tick"):
		return
	if not _require(simulation.get_entities().is_empty(), "initial entities"):
		return
	if not _require(simulation.get_resources().is_empty(), "initial resources"):
		return
	if not _require(simulation.get_power_edges().is_empty(), "initial edges"):
		return
	var empty_adapter: Object = ClassDB.instantiate("FoundationSimulation")
	if not _require(empty_adapter != null, "empty adapter construction failed"):
		return
	empty_adapter = null
	var result: int = simulation.reset_demo()
	if not _require(result == 0, simulation.result_name(result)):
		return
	if not _require(simulation.get_tick() == 2, "unexpected reset tick"):
		return
	var entities: Array = simulation.get_entities()
	var resources: Array = simulation.get_resources()
	var edges: Array = simulation.get_power_edges()
	if not _require(not simulation.has_error(), simulation.get_last_error()):
		return
	if not _require(
		entities.size() == 28,
		"missing presentation entities: got %d" % entities.size()
	):
		return
	var expected_types := [
		1, 2, 3, 2, 2, 2, 1, 2, 3, 2, 2, 2, 4, 2,
		6, 2, 2, 5, 5, 7, 5, 8, 8, 8, 8, 9, 10, 11
	]
	var seen_ids := {}
	for index in entities.size():
		var entity: Dictionary = entities[index]
		var entity_id := int(entity.id)
		if not _require(
			entity_id == index + 1
			and int(entity.type) == expected_types[index]
			and not seen_ids.has(entity_id),
			"native/Godot entity parity failed at index %d" % index
		):
			return
		seen_ids[entity_id] = true
	if not _require(seen_ids.size() == 28, "duplicate or missing stable IDs"):
		return
	var tank: Dictionary = {}
	for entity: Dictionary in entities:
		if int(entity.type) == 10:
			tank = entity
			break
	if not _require(
		not tank.is_empty()
		and tank.fluid_type == 1
		and tank.fluid_quantity == 2500
		and tank.fluid_capacity == 10000,
		"fluid tank presentation: tank=%s entities=%s" % [tank, entities]
	):
		return
	var demo_pipe: Dictionary = entities[27]
	if not _require(
		int(demo_pipe.type) == 11
		and int(demo_pipe.connection_mask) == 2
		and int(demo_pipe.network_id) == 28
		and int(tank.network_id) == 28,
		"pipe/network presentation fields"
	):
		return
	var tank_id := int(tank.id)
	result = simulation.remove_fluid(tank_id, 250)
	if not _require(result == 0, simulation.result_name(result)):
		return
	entities = simulation.get_entities()
	for entity: Dictionary in entities:
		if int(entity.type) == 10:
			tank = entity
			break
	if not _require(
		int(tank.fluid_quantity) == 2250,
		"fluid removal was not reflected through Godot"
	):
		return
	result = simulation.insert_fluid(tank_id, 1, 250)
	if not _require(result == 0, simulation.result_name(result)):
		return
	entities = simulation.get_entities()
	for entity: Dictionary in entities:
		if int(entity.type) == 10:
			tank = entity
			break
	if not _require(
		int(tank.fluid_quantity) == 2500,
		"fluid insertion was not reflected through Godot"
	):
		return
	result = simulation.place_fluid_tank(9, 7)
	if not _require(result == 0, simulation.result_name(result)):
		return
	result = simulation.transfer_fluid(tank_id, 29, 1000)
	if not _require(result == 0, simulation.result_name(result)):
		return
	entities = simulation.get_entities()
	var source_quantity := -1
	var destination_quantity := -1
	for entity: Dictionary in entities:
		if int(entity.id) == tank_id:
			source_quantity = int(entity.fluid_quantity)
		if int(entity.id) == 29:
			destination_quantity = int(entity.fluid_quantity)
	if not _require(
		source_quantity == 1300 and destination_quantity == 1200,
		"fluid transfer was not reflected through Godot"
	):
		return
	if not _require(resources.size() == 2, "missing resources"):
		return
	if not _require(not edges.is_empty(), "missing power edges"):
		return
	var first_id := int(entities[0].id)
	if not _require(first_id > 0, "invalid first entity ID"):
		return
	if not _require(typeof(entities[0].id) == TYPE_INT, "entity ID type"):
		return
	if not _require(typeof(edges[0].a) == TYPE_INT, "power edge ID type"):
		return
	var tick_before := int(simulation.get_tick())
	result = simulation.step()
	if not _require(result == 0, simulation.result_name(result)):
		return
	if not _require(simulation.get_tick() == tick_before + 1, "step count"):
		return
	if not _require(not simulation.get_entities().is_empty(), "post-step data"):
		return
	var events_first: Array = simulation.get_events()
	var events_second: Array = simulation.get_events()
	if not _require(events_first == events_second, "event read cleared batch"):
		return
	for event: Dictionary in events_first:
		if not _require(typeof(event.tick) == TYPE_INT, "event tick type"):
			return
		if not _require(typeof(event.entity_id) == TYPE_INT, "event ID type"):
			return
	simulation.clear_events()
	if not _require(simulation.get_events().is_empty(), "event clear failed"):
		return
	tick_before = int(simulation.get_tick())
	result = simulation.rebuild_presentation()
	if not _require(result == 0, simulation.result_name(result)):
		return
	if not _require(simulation.get_tick() == tick_before, "rebuild advanced tick"):
		return
	if not _require(not simulation.get_entities().is_empty(), "rebuild data"):
		return

	var second: Object = ClassDB.instantiate("FoundationSimulation")
	if not _require(second != null, "second adapter construction failed"):
		return
	if not _require(second.reset_demo() == 0, "second reset failed"):
		return
	if not _require(second.get_tick() == 2, "second adapter tick"):
		return
	if not _require(simulation.get_tick() == tick_before, "adapter interference"):
		return
	second = null

	result = simulation.reset_demo()
	if not _require(result == 0, "repeat reset failed"):
		return
	if not _require(
			int(simulation.get_entities()[0].id) == first_id,
			"repeat reset was not deterministic"
		):
		return
	simulation = null
	print("Foundation Godot smoke test passed")
	quit(0)
