extends Control

const MAX_EVENT_LINES := 80
const STEPS_PER_SECOND := 12.0

@onready var canvas: FoundationFactoryCanvas = %FactoryCanvas
@onready var tick_label: Label = %TickLabel
@onready var status_label: Label = %StatusLabel
@onready var event_log: RichTextLabel = %EventLog
@onready var run_button: Button = %RunButton
@onready var world_controller: Node = %WorldController
@onready var inspector: Control = %EntityInspector
@onready var build_toolbar: HBoxContainer = %BuildToolbar
@onready var mode_label: Label = %ModeLabel
@onready var construction_label: Label = %ConstructionLabel
@onready var build_panel: PanelContainer = $Interface/BuildPanel
@onready var sidebar: VBoxContainer = $Interface/Sidebar
@onready var top_toolbar: PanelContainer = $Interface/Toolbar
const Format := preload("res://scripts/presentation_format.gd")

var simulation: Object
var running := false
var cadence_accumulator := 0.0
var event_lines: Array[String] = []

func _ready() -> void:
	call_deferred("_position_interface")
	var event_font := SystemFont.new()
	event_font.font_names = PackedStringArray(["Menlo", "Monaco", "Monospace"])
	event_log.add_theme_font_override("normal_font", event_font)
	if not ClassDB.class_exists("FoundationSimulation"):
		status_label.text = "Status: native extension failed to load"
		set_physics_process(false)
		return
	simulation = ClassDB.instantiate("FoundationSimulation")
	build_toolbar.build_selected.connect(_on_build_selected)
	build_toolbar.demolish_selected.connect(_on_demolish_selected)
	world_controller.placement_requested.connect(_on_placement_requested)
	world_controller.demolition_requested.connect(_on_demolition_requested)
	world_controller.interaction_mode_changed.connect(_on_interaction_mode_changed)
	inspector.assembler_recipe_requested.connect(_on_assembler_recipe_requested)
	inspector.storage_output_requested.connect(_on_storage_output_requested)
	_reset_demo()
	build_toolbar.configure(simulation)
	inspector.configure_catalogs(
		simulation.get_assembler_recipe_catalog(),simulation.get_item_catalog())

func _notification(what: int) -> void:
	if what == NOTIFICATION_RESIZED and is_node_ready():
		_position_interface()

func _position_interface() -> void:
	var viewport_size := size
	if viewport_size.y < 300.0:
		viewport_size = Vector2(
			float(ProjectSettings.get_setting("display/window/size/viewport_width",1100)),
			float(ProjectSettings.get_setting("display/window/size/viewport_height",720)))
	var sidebar_width := clampf(viewport_size.x * 0.27, 288.0, 420.0)
	sidebar.position = Vector2(viewport_size.x - sidebar_width - 10.0, 10.0)
	sidebar.size = Vector2(sidebar_width, viewport_size.y - 20.0)
	top_toolbar.position = Vector2(12.0, 10.0)
	top_toolbar.size = Vector2(viewport_size.x - sidebar_width - 34.0, 44.0)
	build_panel.position = Vector2(12.0, viewport_size.y - 122.0)
	build_panel.size = Vector2(maxf(360.0, viewport_size.x - sidebar_width - 34.0), 112.0)

func _physics_process(delta: float) -> void:
	if not running:
		return
	cadence_accumulator += delta
	var interval := 1.0 / STEPS_PER_SECOND
	while cadence_accumulator >= interval:
		cadence_accumulator -= interval
		if not _advance(1):
			running = false
			run_button.text = "Run"
			break

func _reset_demo() -> void:
	running = false
	run_button.text = "Run"
	cadence_accumulator = 0.0
	event_lines.clear()
	if is_instance_valid(world_controller):
		world_controller.clear_selection()
		world_controller.enter_select_mode()
	var result: int = simulation.reset_demo()
	_show_result(result)
	_synchronize()

func _advance(count: int) -> bool:
	var result: int = simulation.step_many(count)
	_show_result(result)
	if result != 0:
		return false
	var events: Array = simulation.get_events()
	if simulation.has_error():
		status_label.text = "Status: %s" % simulation.get_last_error()
		return false
	_append_events(events)
	return _synchronize()

func _synchronize() -> bool:
	simulation.clear_error()
	var entities: Array = simulation.get_entities()
	var resources: Array = simulation.get_resources()
	var power_edges: Array = simulation.get_power_edges()
	var tick: int = simulation.get_tick()
	var day: int = simulation.get_day()
	var time_of_day: int = simulation.get_time_of_day()
	if simulation.has_error():
		status_label.text = "Status: %s" % simulation.get_last_error()
		return false
	canvas.synchronize(entities, resources, power_edges)
	world_controller.refresh_selection()
	world_controller.set_hovered_grid(world_controller.hovered_grid)
	build_toolbar.refresh(simulation)
	construction_label.text = "Construction: %d units" % int(simulation.get_construction_units())
	tick_label.text = "Tick: %d  Day: %d  Time: %d" % [
		tick, day, time_of_day
	]
	if world_controller.selected_entity_id == 0:
		var research: Dictionary = simulation.get_research()
		var powered_count := 0
		for entity: Dictionary in entities:
			if bool(entity.get("powered", false)): powered_count += 1
		inspector.show_overview({
			"tick": tick,
			"day": day,
			"construction_units": simulation.get_construction_units(),
			"entity_count": entities.size(),
			"powered_count": powered_count,
			"power_edge_count": power_edges.size(),
			"completed_technology_count": research.get("completed_technology_count", 0),
			"active_mode": _mode_name(world_controller.mode),
		})
	event_log.text = "\n".join(event_lines)
	return true

func _append_events(events: Array) -> void:
	for event: Dictionary in events:
		var row := "t%-4d  type=%-2d  entity=%-3d\n     related=%-3d  item=%-2d  qty=%d" % [
				int(event.tick), int(event.type), int(event.entity_id),
				int(event.related_entity_id), int(event.item_type),
				int(event.quantity)
			]
		var background := "#18202a" if event_lines.size() % 2 == 0 else "#202935"
		var accent := "#65d3e7" if int(event.type) >= 20 else "#e5c85a"
		event_lines.append("[bgcolor=%s][color=%s]▎[/color][font_size=12] %s [/font_size][/bgcolor]" % [background, accent, row])
	while event_lines.size() > MAX_EVENT_LINES:
		event_lines.pop_front()

func _show_result(result: int) -> void:
	status_label.text = "Status: %s" % simulation.result_name(result)

func _on_reset_pressed() -> void:
	_reset_demo()

func _on_step_pressed() -> void:
	_advance(1)

func _on_run_pressed() -> void:
	running = not running
	run_button.text = "Pause" if running else "Run"

func _on_build_selected(entity_type: int) -> void:
	world_controller.enter_build_mode(entity_type)

func _on_demolish_selected() -> void:
	world_controller.enter_demolish_mode()

func _on_interaction_mode_changed(mode: int,entity_type: int) -> void:
	build_toolbar.set_active(entity_type,mode == 2)
	if mode == 1:
		mode_label.text = "Mode: Build %s — R rotates, Esc cancels" % Format.entity_type(entity_type)
	elif mode == 2:
		mode_label.text = "Mode: Demolish — Esc cancels"
	else:
		mode_label.text = "Mode: Select"
	if world_controller.selected_entity_id == 0 and simulation != null:
		_synchronize()

func _mode_name(mode: int) -> String:
	if mode == 1: return "Build"
	if mode == 2: return "Demolish"
	return "Select"

func _on_placement_requested(entity_type: int,grid: Vector2i,direction: int) -> void:
	var queued: int = simulation.queue_place_entity(entity_type,grid.x,grid.y,direction)
	if queued != 0:
		status_label.text = "Status: %s" % simulation.result_name(queued)
		return
	_execute_queued_command("Placed %s" % Format.entity_type(entity_type))

func _on_demolition_requested(entity_id: int) -> void:
	var queued: int = simulation.queue_demolish_entity(entity_id)
	if queued != 0:
		status_label.text = "Status: %s" % simulation.result_name(queued)
		return
	_execute_queued_command("Demolished entity #%d" % entity_id)

func _on_assembler_recipe_requested(entity_id: int,recipe_id: int) -> void:
	var queued: int = simulation.queue_set_assembler_recipe(entity_id,recipe_id)
	if queued != 0:
		status_label.text = "Status: %s" % simulation.result_name(queued)
		return
	_execute_queued_command("Assembler recipe updated")

func _on_storage_output_requested(entity_id: int,item_type: int) -> void:
	var queued: int = simulation.queue_set_storage_output(entity_id,item_type)
	if queued != 0:
		status_label.text = "Status: %s" % simulation.result_name(queued)
		return
	_execute_queued_command("Storage output updated")

func _execute_queued_command(success_message: String) -> void:
	if not _advance(1): return
	var results: Array = simulation.get_command_results()
	if results.is_empty():
		status_label.text = "Status: command produced no result"
		return
	var command_result: Dictionary = results.back()
	var result := int(command_result.result)
	status_label.text = "Status: %s" % (success_message if result == 0 else simulation.result_name(result))
