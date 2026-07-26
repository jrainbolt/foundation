extends SceneTree

func _fail(message: String) -> void:
	push_error(message)
	quit(1)

func _initialize() -> void:
	call_deferred("_run")

func _run() -> void:
	var packed: PackedScene = load("res://scenes/main.tscn")
	if packed == null:
		_fail("main scene did not load")
		return
	var main: Control = packed.instantiate()
	root.add_child(main)
	await process_frame
	await physics_frame

	if main.simulation == null:
		_fail("main scene did not construct adapter")
		return
	var initial_tick := int(main.simulation.get_tick())
	var canvas: Node = main.canvas
	if initial_tick != 2 or canvas.entity_nodes.size() != 34:
		_fail("deterministic demo or entity visuals are incorrect")
		return
	var tank_count := 0
	for entity_id: int in canvas.entity_nodes:
		var visual: FoundationEntityVisual = canvas.entity_nodes[entity_id]
		if int(visual.state.get("type", 0)) == 10:
			tank_count += 1
			if int(visual.state.get("fluid_type", 0)) != 1 \
					or int(visual.state.get("fluid_quantity", 0)) != 2500 \
					or int(visual.state.get("fluid_capacity", 0)) != 10000:
				_fail("fluid tank visual has incorrect presentation fields")
				return
	if tank_count != 1:
		_fail("main scene did not receive exactly one fluid tank")
		return
	var pipe_count := 0
	for entity_id: int in canvas.entity_nodes:
		var visual: FoundationEntityVisual = canvas.entity_nodes[entity_id]
		if int(visual.state.get("type", 0)) == 11:
			pipe_count += 1
			if int(visual.state.get("connection_mask", 0)) == 0 \
					or int(visual.state.get("network_id", 0)) == 0:
				_fail("pipe visual has incorrect network fields")
				return
	if pipe_count != 3:
		_fail("main scene did not receive exactly three pipes")
		return
	var water_extractor_count := 0
	var boiler_count := 0
	var steam_engine_count := 0
	var solar_generator_count := 0
	for entity_id: int in canvas.entity_nodes:
		var visual: FoundationEntityVisual = canvas.entity_nodes[entity_id]
		if int(visual.state.get("type", 0)) == 12:
			water_extractor_count += 1
			if int(visual.state.get("stored_water", -1)) != 0 \
					or int(visual.state.get("output_capacity", 0)) != 1000:
				_fail("water extractor visual has incorrect fields")
				return
		elif int(visual.state.get("type", 0)) == 13:
			boiler_count += 1
			if int(visual.state.get("stored_water", -1)) != 0 \
					or int(visual.state.get("stored_steam", -1)) != 0 \
					or bool(visual.state.get("fuel_active", true)) \
					or bool(visual.state.get("conversion_active", true)):
				_fail("boiler visual has incorrect fields")
				return
		elif int(visual.state.get("type", 0)) == 14:
			steam_engine_count += 1
			if int(visual.state.get("stored_steam", -1)) != 0 \
					or int(visual.state.get("steam_capacity", 0)) != 1000 \
					or int(visual.state.get("steam_network_id", 0)) == 0 \
					or int(visual.state.get("power_network_id", 0)) == 0:
				_fail("steam engine visual has incorrect fields")
				return
		elif int(visual.state.get("type", 0)) == 15:
			solar_generator_count += 1
			if int(visual.state.get("maximum_output", 0)) != 100 \
					or int(visual.state.get("available_generation", -1)) != 0:
				_fail("solar generator visual has incorrect fields")
				return
	if water_extractor_count != 1 or boiler_count != 1 \
			or steam_engine_count != 1 or solar_generator_count != 1:
		_fail("main scene did not receive fluid machines")
		return
	if canvas.resources.size() != 2 or canvas.edges.is_empty():
		_fail("resource or power-edge visuals are missing")
		return
	if not main.tick_label.text.contains("2"):
		_fail("debug tick panel did not update")
		return

	main._on_run_pressed()
	for unused in range(12):
		await physics_frame
	var running_tick := int(main.simulation.get_tick())
	if running_tick <= initial_tick:
		_fail("run mode did not advance")
		return

	main._on_run_pressed()
	var paused_tick := int(main.simulation.get_tick())
	for unused in range(12):
		await physics_frame
	if int(main.simulation.get_tick()) != paused_tick:
		_fail("pause did not stop advancement")
		return

	main._on_step_pressed()
	if int(main.simulation.get_tick()) != paused_tick + 1:
		_fail("single step did not advance exactly once")
		return

	main._on_reset_pressed()
	if int(main.simulation.get_tick()) != initial_tick:
		_fail("reset did not restore initial tick")
		return
	if canvas.entity_nodes.size() != 34 or canvas.resources.size() != 2:
		_fail("reset did not restore deterministic visuals")
		return

	main.queue_free()
	await process_frame
	var reloaded: Control = packed.instantiate()
	root.add_child(reloaded)
	await process_frame
	await physics_frame
	if reloaded.simulation == null or reloaded.simulation.get_tick() != initial_tick:
		_fail("scene reload did not recreate deterministic adapter state")
		return
	reloaded.queue_free()
	await process_frame
	print("Foundation main scene smoke test passed")
	quit(0)
