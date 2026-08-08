extends SceneTree

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
	if not require_value(camera != null and controller != null and inspector != null, "interaction nodes"): return
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
