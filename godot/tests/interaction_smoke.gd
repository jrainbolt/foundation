extends SceneTree

const BuildToolbar := preload("res://scripts/build_toolbar.gd")

func require_value(condition: bool, message: String) -> bool:
	if condition: return true
	push_error(message)
	quit(1)
	return false

func _initialize() -> void:
	call_deferred("run_test")

func run_test() -> void:
	var scene: PackedScene = load("res://scenes/main.tscn")
	if not require_value(scene != null, "main scene load"): return
	var main: Control = scene.instantiate()
	root.add_child(main)
	await process_frame
	var simulation: Object = main.simulation
	var canvas: Node = main.canvas
	var controller: Node = main.world_controller
	var inspector: Control = main.inspector
	var camera: Camera2D = main.get_node("World/Camera2D")
	var toolbar: HBoxContainer = main.build_toolbar
	if not require_value(camera != null and controller != null and inspector != null, "interaction nodes"): return
	var uninitialized: Object = ClassDB.instantiate("FoundationSimulation")
	var locked_definition: Dictionary = {}
	for definition: Dictionary in uninitialized.get_build_catalog():
		if int(definition.entity_type) == 21: locked_definition = definition
	if not require_value(not locked_definition.is_empty() and int(locked_definition.required_unlock) != 0 and not bool(locked_definition.unlocked), "locked catalog metadata"): return
	var locked_toolbar: HBoxContainer = BuildToolbar.new()
	locked_toolbar.configure(uninitialized)
	if not require_value(locked_toolbar.buttons[21].disabled, "locked toolbar button state"): return
	locked_toolbar.free()
	uninitialized = null
	controller.enter_build_mode(5)
	if not require_value(controller.mode == 1 and controller.build_entity_type == 5, "enter build mode"): return
	controller.hovered_grid = Vector2i(12,7)
	if not require_value(controller.preview_is_advisably_valid(), "empty placement preview"): return
	controller.hovered_grid = Vector2i(0,2)
	if not require_value(not controller.preview_is_advisably_valid(), "occupied placement preview"): return
	controller.rotate_build()
	if not require_value(controller.build_direction == 1, "build rotation"): return
	controller.enter_demolish_mode()
	if not require_value(controller.mode == 2, "enter demolition mode"): return
	controller.enter_select_mode()
	if not require_value(controller.mode == 0, "exit interaction mode"): return
	if not require_value(toolbar.buttons.size() >= 11 and toolbar.buttons[5].disabled == false, "build toolbar catalog"): return
	var tick_before := int(simulation.get_tick())
	var camera_before := camera.position
	camera.move_by(Vector2(25, -10))
	camera.set_zoom_level(100.0)
	if not require_value(camera.position != camera_before and is_equal_approx(camera.zoom.x, camera.maximum_zoom), "camera movement or maximum zoom"): return
	camera.set_zoom_level(0.01)
	if not require_value(is_equal_approx(camera.zoom.x, camera.minimum_zoom) and int(simulation.get_tick()) == tick_before, "minimum zoom or camera advanced simulation"): return
	if not require_value(controller.world_to_grid(Vector2(-1, -1)) == Vector2i(-1, -1), "negative grid conversion"): return
	if not require_value(controller.grid_to_world(Vector2i(2, 3)) == Vector2(128, 192), "grid world conversion"): return
	var first_state: Dictionary = canvas.entity_nodes[1].state
	var first_grid := Vector2i(int(first_state.x), int(first_state.y))
	if not require_value(controller.pick_grid(first_grid) == 1 and controller.pick_grid(Vector2i(-20, -20)) == 0, "deterministic grid picking"): return
	if not require_value(controller.select_entity(1), "known entity selection"): return
	if not require_value(controller.selected_entity_id == 1 and canvas.entity_nodes[1].selected, "stable selected ID or styling"): return
	if not require_value(inspector.entity_id == 1 and inspector.details.text.contains("Entity ID: #1"), "production inspector identity"): return
	controller.select_entity(5)
	if not require_value(not canvas.entity_nodes[1].selected and canvas.entity_nodes[5].selected, "selection switching"): return
	if not require_value(inspector.details.text.contains("Inventory and storage"), "storage inspector section"): return
	controller.select_entity(26)
	if not require_value(inspector.details.text.contains("Network"), "power inspector data"): return
	controller.select_entity(27)
	if not require_value(inspector.details.text.contains("Fluid Quantity"), "fluid inspector data"): return
	var selected: int = int(controller.selected_entity_id)
	if not require_value(main._advance(1), "simulation step"): return
	if not require_value(controller.selected_entity_id == selected and canvas.entity_nodes[selected].selected, "selection synchronization"): return
	if not require_value(inspector.entity_id == selected and int(simulation.get_tick()) == tick_before + 1, "inspector refresh"): return
	controller.select_entity(1)
	var entity_count: int = simulation.get_entities().size()
	if not require_value(simulation.queue_place_entity(5,12,7,0) == 0, "placement queue submission"): return
	if not require_value(simulation.get_entities().size() == entity_count, "placement mutated before tick"): return
	if not require_value(simulation.step() == 0, "placement execution tick"): return
	var command_results: Array = simulation.get_command_results()
	if not require_value(command_results.size() == 1 and int(command_results[0].result) == 0, "successful placement result"): return
	if not require_value(main._synchronize() and simulation.get_entities().size() == entity_count + 1, "placement presentation synchronization"): return
	if not require_value(controller.selected_entity_id == 1, "selection did not survive placement"): return
	if not require_value(simulation.queue_place_entity(2,12,7,1) == 0 and simulation.step() == 0, "occupied placement execution"): return
	command_results = simulation.get_command_results()
	if not require_value(int(command_results[0].result) != 0 and simulation.get_entities().size() == entity_count + 1, "occupied placement rejection"): return
	if not require_value(main._synchronize() and controller.select_entity(49), "select constructed entity"): return
	if not require_value(simulation.queue_demolish_entity(49) == 0, "demolition queue submission"): return
	if not require_value(simulation.get_entities().size() == entity_count + 1, "demolition mutated before tick"): return
	if not require_value(simulation.step() == 0 and main._synchronize(), "demolition execution and synchronization"): return
	command_results = simulation.get_command_results()
	if not require_value(int(command_results[0].result) == 0 and simulation.get_entities().size() == entity_count, "successful demolition result"): return
	if not require_value(controller.selected_entity_id == 0 and inspector.entity_id == 0, "demolished selection clearing"): return
	if not require_value(simulation.queue_demolish_entity(999999) == 0 and simulation.step() == 0, "failed demolition execution"): return
	command_results = simulation.get_command_results()
	if not require_value(int(command_results[0].result) != 0, "failed demolition result"): return
	if not require_value(not controller.select_entity(999999) and controller.selected_entity_id == 0, "invalid selection safety"): return
	if not require_value(inspector.details.text.contains("Select an entity"), "inspector empty state"): return
	controller.select_entity(1)
	main._reset_demo()
	if not require_value(controller.selected_entity_id == 0 and inspector.entity_id == 0, "reset selection policy"): return
	if not require_value(canvas.entity_nodes.size() == 48, "reset visual parity"): return
	main.queue_free()
	await process_frame
	print("Foundation interaction smoke test passed")
	quit(0)
