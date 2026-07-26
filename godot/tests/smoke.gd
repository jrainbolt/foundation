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
	if not _require(entities.size() == 26, "missing presentation entities"):
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
