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
@onready var sidebar_backdrop: ColorRect = $Interface/SidebarBackdrop
@onready var top_toolbar: PanelContainer = $Interface/Toolbar
@onready var research_summary: Label = %ResearchSummary
@onready var goal_label: Label = %GoalLabel
@onready var selection_summary: Label = %SelectionSummary
@onready var alert_button: Button = %AlertButton
@onready var history_button: Button = %HistoryButton
@onready var research_panel: Control = %ResearchPanel
@onready var alert_panel: Control = %AlertPanel
@onready var notification_label: Label = %NotificationLabel
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
	world_controller.selected_entity_changed.connect(_on_selected_entity_changed)
	inspector.assembler_recipe_requested.connect(_on_assembler_recipe_requested)
	inspector.refinery_recipe_requested.connect(_on_refinery_recipe_requested)
	inspector.storage_output_requested.connect(_on_storage_output_requested)
	inspector.rail_switch_branch_requested.connect(_on_rail_switch_branch_requested)
	inspector.train_destination_requested.connect(_on_train_destination_requested)
	inspector.rail_station_freight_mode_requested.connect(_on_rail_station_freight_mode_requested)
	inspector.rail_station_freight_item_requested.connect(_on_rail_station_freight_item_requested)
	inspector.train_schedule_add_requested.connect(_on_train_schedule_add_requested)
	inspector.train_schedule_remove_requested.connect(_on_train_schedule_remove_requested)
	inspector.train_schedule_clear_requested.connect(_on_train_schedule_clear_requested)
	inspector.train_schedule_enabled_requested.connect(_on_train_schedule_enabled_requested)
	inspector.couple_rear_wagon_requested.connect(_on_couple_rear_wagon_requested)
	inspector.decouple_rear_wagon_requested.connect(_on_decouple_rear_wagon_requested)
	%ResearchButton.pressed.connect(_toggle_research)
	alert_button.pressed.connect(_toggle_alerts)
	history_button.pressed.connect(_toggle_history)
	research_panel.research_requested.connect(_on_research_requested)
	alert_panel.entity_requested.connect(_on_alert_entity_requested)
	_reset_demo()
	build_toolbar.configure(simulation)
	inspector.configure_catalogs(
		simulation.get_assembler_recipe_catalog(),simulation.get_item_catalog(),
		simulation.get_refinery_recipe_catalog())

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
	sidebar_backdrop.position = sidebar.position - Vector2(8.0,0.0)
	sidebar_backdrop.size = sidebar.size + Vector2(8.0,0.0)
	top_toolbar.position = Vector2(12.0, 10.0)
	top_toolbar.size = Vector2(viewport_size.x - sidebar_width - 34.0, 44.0)
	build_panel.position = Vector2(12.0, viewport_size.y - 166.0)
	build_panel.size = Vector2(maxf(360.0, viewport_size.x - sidebar_width - 34.0), 156.0)

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
	notification_label.text = ""
	research_panel.visible = false
	alert_panel.visible = false
	if is_instance_valid(world_controller):
		world_controller.clear_selection()
		world_controller.enter_select_mode()
	var result: int = simulation.reset_demo()
	_show_result(result)
	_synchronize()
	%Camera2D.center_on_grid(
		Vector2i(simulation.get_start_x(),simulation.get_start_y()),76.0)
	%Camera2D.set_zoom_level(0.55)

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
	for entity: Dictionary in entities:
		entity["telemetry"] = simulation.get_entity_telemetry(int(entity.id))
	var resources: Array = simulation.get_resources()
	var terrain: Array = simulation.get_terrain()
	var power_edges: Array = simulation.get_power_edges()
	var tick: int = simulation.get_tick()
	var day: int = simulation.get_day()
	var time_of_day: int = simulation.get_time_of_day()
	if simulation.has_error():
		status_label.text = "Status: %s" % simulation.get_last_error()
		return false
	canvas.synchronize(entities, resources, power_edges, terrain)
	var stations: Array = []
	for entity: Dictionary in entities:
		if int(entity.get("type",0)) == 25: stations.append(entity)
	inspector.configure_stations(stations)
	var selected_id := int(world_controller.selected_entity_id)
	canvas.set_selected_route(simulation.get_train_route(selected_id)
		if selected_id != 0 else [])
	var selected_state: Dictionary = {}
	for entity: Dictionary in entities:
		if int(entity.id) == selected_id:
			selected_state = entity
			break
	var wagons: Array = []
	for entity: Dictionary in entities:
		if int(entity.get("type",0)) == 28: wagons.append(entity)
	wagons.sort_custom(func(a: Dictionary,b: Dictionary) -> bool:
		return int(a.get("id",0)) < int(b.get("id",0)))
	selection_summary.text = "Selected: None" if selected_state.is_empty() else \
		"Selected: %s #%d — %s" % [Format.entity_type(int(selected_state.get("type",0))),
		selected_id,Format.machine_status(int(selected_state.get("status",0)))]
	if int(selected_state.get("type",0)) == 27:
		inspector.configure_train_schedule(
			simulation.get_train_schedule(selected_id),
			bool(selected_state.get("schedule_enabled",false)))
	var waiting := int(selected_state.get("reservation_status",0)) == 2
	canvas.set_selected_train_blocks(
		int(selected_state.get("current_block_id",0)),
		selected_state.get("reserved_blocks",[]),
		int(selected_state.get("next_route_block_id",0)) if waiting else 0)
	world_controller.refresh_selection()
	inspector.configure_consist(selected_state,wagons)
	world_controller.set_hovered_grid(world_controller.hovered_grid)
	build_toolbar.refresh(simulation)
	var depot_supply := 0
	var depot_count := 0
	for entity: Dictionary in entities:
		if int(entity.get("type",0)) == 23:
			depot_count += 1
			depot_supply += int(entity.get("material_quantity",0))
	construction_label.text = "%s: %s units" % [
		"Depot supply" if depot_count > 0 else "Bootstrap supply",
		Format.number(depot_supply if depot_count > 0 else int(simulation.get_construction_units()))]
	tick_label.text = "Tick: %d  Day: %d  Time: %d" % [
		tick, day, time_of_day
	]
	var research_catalog: Array = simulation.get_technology_catalog()
	research_panel.refresh(research_catalog)
	var active_name := "None"
	var next_name := "Factory complete"
	for technology: Dictionary in research_catalog:
		if bool(technology.active): active_name = Format.technology(int(technology.technology_id))
		if next_name == "Factory complete" and not bool(technology.completed):
			next_name = Format.technology(int(technology.technology_id))
	var research: Dictionary = simulation.get_research()
	research_summary.text = "Research: %s  %d/%d" % [active_name,
		int(research.get("completed_units",0)),int(research.get("required_units",0))]
	goal_label.text = "Next: %s" % next_name
	var alert_groups := _derive_alerts(entities)
	alert_panel.refresh(alert_groups)
	alert_button.text = "Alerts %d" % alert_panel.alert_count
	if world_controller.selected_entity_id == 0:
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
		match int(event.type):
			38: notification_label.text = "Technology completed: %s" % Format.technology(int(event.technology_id))
			41: notification_label.text = "Resource deposit depleted"
			47: notification_label.text = "Train route invalidated — inspect the train"
			48: notification_label.text = "Train arrived at station"
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

func _toggle_research() -> void:
	research_panel.visible = not research_panel.visible
	if research_panel.visible: alert_panel.visible = false

func _toggle_alerts() -> void:
	alert_panel.visible = not alert_panel.visible
	if alert_panel.visible: research_panel.visible = false

func _toggle_history() -> void:
	event_log.visible = not event_log.visible
	$Interface/Sidebar/LogLabel.visible = event_log.visible
	history_button.text = "Hide History" if event_log.visible else "History"

func _on_research_requested(technology_id: int) -> void:
	var queued := int(simulation.queue_select_research(technology_id))
	if queued != 0:
		_show_result(queued)
		return
	research_panel.visible = false
	_execute_queued_command("Research selected: %s" % Format.technology(technology_id))

func _on_alert_entity_requested(entity_id: int) -> void:
	if not world_controller.select_entity(entity_id): return
	var visual: FoundationEntityVisual = canvas.entity_nodes[entity_id]
	%Camera2D.center_on_grid(Vector2i(int(visual.state.x),int(visual.state.y)),76.0)
	alert_panel.visible = false

func _derive_alerts(entities: Array) -> Dictionary:
	var groups := {"Unpowered":[],"Blocked input":[],"Blocked output":[],
		"Depleted resource":[],"Train waiting":[],"Invalid route":[],
		"Disconnected station":[],"Research missing science":[]}
	for entity: Dictionary in entities:
		var id := int(entity.get("id",0))
		match int(entity.get("status",0)):
			3: groups["Blocked input"].append(id)
			4: groups["Blocked output"].append(id)
			5: groups["Unpowered"].append(id)
		if int(entity.get("type",0)) == 1 and int(entity.get("resource_remaining",1)) == 0:
			groups["Depleted resource"].append(id)
		if int(entity.get("type",0)) == 27:
			if int(entity.get("reservation_status",0)) == 2: groups["Train waiting"].append(id)
			if int(entity.get("route_status",0)) == 3: groups["Invalid route"].append(id)
		if int(entity.get("type",0)) == 25 and not bool(entity.get("rail_connected",false)):
			groups["Disconnected station"].append(id)
		if int(entity.get("type",0)) == 22 and int(entity.get("activity",0)) == 4:
			groups["Research missing science"].append(id)
	return groups

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
	var queued: int
	if entity_type == 27:
		var rail_id: int = world_controller.pick_grid(grid)
		queued = simulation.queue_place_locomotive(rail_id,direction)
	elif entity_type == 28:
		var rail_id: int = world_controller.pick_grid(grid)
		queued = simulation.queue_place_cargo_wagon(rail_id,direction)
	else:
		queued = simulation.queue_place_entity(entity_type,grid.x,grid.y,direction)
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

func _on_refinery_recipe_requested(entity_id: int,recipe_id: int) -> void:
	var queued: int = simulation.queue_set_refinery_recipe(entity_id,recipe_id)
	if queued != 0:
		status_label.text = "Status: %s" % simulation.result_name(queued)
		return
	_execute_queued_command("Refinery recipe updated")

func _on_selected_entity_changed(entity_id: int) -> void:
	var selected_state: Dictionary = {}
	var wagons: Array = []
	for entity: Dictionary in simulation.get_entities():
		if int(entity.get("type",0)) == 28:
			wagons.append(entity)
		if int(entity.get("id",0)) == entity_id:
			selected_state = entity
	wagons.sort_custom(func(a: Dictionary,b: Dictionary) -> bool:
		return int(a.get("id",0)) < int(b.get("id",0)))
	inspector.configure_consist(selected_state,wagons)

func _on_couple_rear_wagon_requested(locomotive_id: int,wagon_id: int) -> void:
	var queued: int = simulation.queue_couple_rear_wagon(locomotive_id,wagon_id)
	if queued != 0:
		status_label.text = "Status: %s" % simulation.result_name(queued)
		return
	_execute_queued_command("Rear wagon coupled")

func _on_decouple_rear_wagon_requested(locomotive_id: int) -> void:
	var queued: int = simulation.queue_decouple_rear_wagon(locomotive_id)
	if queued != 0:
		status_label.text = "Status: %s" % simulation.result_name(queued)
		return
	_execute_queued_command("Rear wagon decoupled")

func _on_storage_output_requested(entity_id: int,item_type: int) -> void:
	var queued: int = simulation.queue_set_storage_output(entity_id,item_type)
	if queued != 0:
		status_label.text = "Status: %s" % simulation.result_name(queued)
		return
	_execute_queued_command("Storage output updated")

func _on_rail_switch_branch_requested(entity_id: int,branch: int) -> void:
	var queued: int = simulation.queue_set_rail_switch_branch(entity_id,branch)
	if queued != 0:
		status_label.text = "Status: %s" % simulation.result_name(queued)
		return
	_execute_queued_command("Rail switch branch updated")

func _on_train_destination_requested(entity_id: int,station_id: int) -> void:
	var queued: int = simulation.queue_clear_train_destination(entity_id) \
		if station_id == 0 else simulation.queue_set_train_destination(entity_id,station_id)
	if queued != 0:
		status_label.text = "Status: %s" % simulation.result_name(queued)
		return
	_execute_queued_command("Train destination updated")

func _on_rail_station_freight_mode_requested(entity_id: int,mode: int) -> void:
	var queued: int = simulation.queue_set_rail_station_freight_mode(entity_id,mode)
	if queued != 0:
		status_label.text = "Status: %s" % simulation.result_name(queued)
		return
	_execute_queued_command("Station freight mode updated")

func _on_rail_station_freight_item_requested(entity_id: int,item_type: int) -> void:
	var queued: int = simulation.queue_set_rail_station_freight_item(entity_id,item_type)
	if queued != 0:
		status_label.text = "Status: %s" % simulation.result_name(queued)
		return
	_execute_queued_command("Station freight item updated")

func _on_train_schedule_add_requested(entity_id: int,station_id: int,
		wait_condition: int,wait_value: int) -> void:
	var queued: int = simulation.queue_train_schedule_add_stop(
		entity_id,station_id,wait_condition,wait_value)
	if queued != 0:
		status_label.text = "Status: %s" % simulation.result_name(queued)
		return
	_execute_queued_command("Train schedule stop added")

func _on_train_schedule_remove_requested(entity_id: int,index: int) -> void:
	var queued: int = simulation.queue_train_schedule_remove_stop(entity_id,index)
	if queued != 0:
		status_label.text = "Status: %s" % simulation.result_name(queued)
		return
	_execute_queued_command("Train schedule stop removed")

func _on_train_schedule_clear_requested(entity_id: int) -> void:
	var queued: int = simulation.queue_train_schedule_clear(entity_id)
	if queued != 0:
		status_label.text = "Status: %s" % simulation.result_name(queued)
		return
	_execute_queued_command("Train schedule cleared")

func _on_train_schedule_enabled_requested(entity_id: int,enabled: bool) -> void:
	var queued: int = simulation.queue_train_schedule_set_enabled(entity_id,enabled)
	if queued != 0:
		status_label.text = "Status: %s" % simulation.result_name(queued)
		return
	_execute_queued_command("Train schedule updated")

func _execute_queued_command(success_message: String) -> void:
	if not _advance(1): return
	var results: Array = simulation.get_command_results()
	if results.is_empty():
		status_label.text = "Status: command produced no result"
		return
	var command_result: Dictionary = results.back()
	var result := int(command_result.result)
	status_label.text = "Status: %s" % (success_message if result == 0 else simulation.result_name(result))
