class_name FoundationEntityInspector
extends PanelContainer

signal assembler_recipe_requested(entity_id: int,recipe_id: int)
signal storage_output_requested(entity_id: int,item_type: int)
signal rail_switch_branch_requested(entity_id: int,branch: int)
signal train_destination_requested(entity_id: int,station_id: int)
signal rail_station_freight_mode_requested(entity_id: int,mode: int)
signal rail_station_freight_item_requested(entity_id: int,item_type: int)
signal train_schedule_add_requested(entity_id: int,station_id: int,wait_condition: int,wait_value: int)
signal train_schedule_remove_requested(entity_id: int,index: int)
signal train_schedule_clear_requested(entity_id: int)
signal train_schedule_enabled_requested(entity_id: int,enabled: bool)

const Format := preload("res://scripts/presentation_format.gd")
@onready var title_label: Label = %InspectorTitle
@onready var details: RichTextLabel = %InspectorDetails
@onready var configuration_label: Label = %ConfigurationLabel
@onready var configuration_selector: OptionButton = %ConfigurationSelector
@onready var secondary_configuration_label: Label = %SecondaryConfigurationLabel
@onready var secondary_configuration_selector: OptionButton = %SecondaryConfigurationSelector
@onready var schedule_panel: VBoxContainer = %SchedulePanel
@onready var schedule_enabled: CheckButton = %ScheduleEnabled
@onready var schedule_stop_selector: OptionButton = %ScheduleStopSelector
@onready var schedule_station_selector: OptionButton = %ScheduleStationSelector
@onready var schedule_wait_selector: OptionButton = %ScheduleWaitSelector
@onready var schedule_wait_ticks: SpinBox = %ScheduleWaitTicks
@onready var schedule_add: Button = %ScheduleAdd
@onready var schedule_remove: Button = %ScheduleRemove
@onready var schedule_clear: Button = %ScheduleClear
var entity_id := 0
var recipe_catalog: Array = []
var item_catalog: Array = []
var station_catalog: Array = []
var configuring := false

func _ready() -> void:
	configuration_selector.item_selected.connect(_on_configuration_selected)
	secondary_configuration_selector.item_selected.connect(_on_secondary_configuration_selected)
	schedule_enabled.toggled.connect(_on_schedule_enabled_toggled)
	schedule_wait_selector.item_selected.connect(_on_schedule_wait_selected)
	schedule_add.pressed.connect(_on_schedule_add_pressed)
	schedule_remove.pressed.connect(_on_schedule_remove_pressed)
	schedule_clear.pressed.connect(_on_schedule_clear_pressed)
	for wait_name in ["None", "Time", "Cargo empty", "Cargo full"]:
		schedule_wait_selector.add_item(wait_name)

func configure_catalogs(recipes: Array,items: Array) -> void:
	recipe_catalog = recipes.duplicate(true)
	item_catalog = items.duplicate(true)

func configure_stations(stations: Array) -> void:
	station_catalog = stations.duplicate(true)

func configure_train_schedule(stops: Array,enabled: bool) -> void:
	configuring = true
	schedule_stop_selector.clear()
	for stop: Dictionary in stops:
		var wait_condition := int(stop.get("wait_condition",0))
		var wait_name: String = ["none","time","cargo empty","cargo full"][
			clampi(wait_condition,0,3)]
		schedule_stop_selector.add_item("%d: Station #%d — %s" % [
			int(stop.get("index",0))+1,int(stop.get("station_id",0)),wait_name])
	schedule_station_selector.clear()
	for station: Dictionary in station_catalog:
		var station_id := int(station.get("id",0))
		schedule_station_selector.add_item("Station #%d" % station_id)
		schedule_station_selector.set_item_metadata(
			schedule_station_selector.item_count-1,station_id)
	schedule_enabled.button_pressed = enabled
	var editable := not enabled
	schedule_station_selector.disabled = not editable
	schedule_wait_selector.disabled = not editable
	schedule_wait_ticks.editable = editable
	schedule_add.disabled = not editable or stops.size() >= 16 or station_catalog.is_empty()
	schedule_remove.disabled = not editable or stops.is_empty()
	schedule_clear.disabled = not editable or stops.is_empty()
	_on_schedule_wait_selected(schedule_wait_selector.selected)
	configuring = false

func clear_entity() -> void:
	entity_id = 0
	_hide_configuration()
	title_label.text = "Foundation"
	details.text = "[b]Simulation overview[/b]\nSynchronizing presentation data…"

func show_overview(overview: Dictionary) -> void:
	entity_id = 0
	_hide_configuration()
	title_label.text = "Foundation"
	var lines: Array[String] = []
	section(lines, "Simulation status")
	field(lines, "Tick", Format.number(int(overview.get("tick", 0))))
	field(lines, "Day", Format.number(int(overview.get("day", 0))))
	field(lines, "Construction", "%s units" % Format.number(int(overview.get("construction_units", 0))))
	section(lines, "Factory overview")
	field(lines, "Entities", Format.number(int(overview.get("entity_count", 0))))
	field(lines, "Power", "%d powered · %d links" % [int(overview.get("powered_count", 0)), int(overview.get("power_edge_count", 0))])
	field(lines, "Research", "%d technologies complete" % int(overview.get("completed_technology_count", 0)))
	field(lines, "Active mode", str(overview.get("active_mode", "Select")))
	field(lines, "Selection", "None")
	details.text = "\n".join(lines)

func show_entity(state: Dictionary) -> void:
	if state.is_empty():
		clear_entity()
		return
	entity_id = int(state.get("id", 0))
	var type_id := int(state.get("type", 0))
	title_label.text = Format.entity_type(type_id)
	_show_configuration(type_id,state)
	var lines: Array[String] = []
	section(lines, "Identity")
	field(lines, "Entity ID", "#%d" % entity_id)
	field(lines, "Position", "(%d, %d)" % [int(state.get("x", 0)), int(state.get("y", 0))])
	if state.has("direction"): field(lines, "Direction", Format.direction(int(state.direction)))
	section(lines, "Status")
	if state.has("status"): field(lines, "Status", Format.machine_status(int(state.status)))
	if state.has("powered"): field(lines, "Powered", Format.yes_no(bool(state.powered)))
	for key in ["network", "power_network_id", "network_id", "input_network_id", "output_network_id", "steam_network_id", "water_network_id", "heat_network_id"]:
		if state.has(key): field(lines, key.capitalize(), str(int(state[key])))
	if state.has("rail_network_id"): field(lines,"Rail network",str(int(state.rail_network_id)))
	if state.has("rail_geometry"): field(lines,"Geometry",str(int(state.rail_geometry)))
	if state.has("switch_geometry"): field(lines,"Switch geometry",str(int(state.switch_geometry)))
	if state.has("selected_branch"): field(lines,"Selected branch","A" if int(state.selected_branch) == 0 else "B")
	if state.has("connection_mask"): field(lines,"Connection mask",str(int(state.connection_mask)))
	if state.has("rail_neighbors"): field(lines,"Neighbors",str(state.rail_neighbors))
	if state.has("rail_connected"): field(lines,"Rail connected",Format.yes_no(bool(state.rail_connected)))
	if state.has("attached_rail_id"): field(lines,"Attached rail","#%d" % int(state.attached_rail_id))
	if state.has("freight_mode"): field(lines,"Freight mode",["DISABLED","LOAD","UNLOAD"][clampi(int(state.freight_mode),0,2)])
	if state.has("freight_item"): field(lines,"Freight item",Format.item(int(state.freight_item)))
	if state.has("freight_quantity"): field(lines,"Station freight","%s / %s" % [Format.number(int(state.freight_quantity)),Format.number(int(state.get("freight_capacity",0)))])
	if state.has("eligible_train_id"): field(lines,"Eligible train","None" if int(state.eligible_train_id) == 0 else "#%d" % int(state.eligible_train_id))
	if state.has("signal_orientation"): field(lines,"Controlled direction",Format.direction(int(state.signal_orientation)))
	if state.has("upstream_block_id"): field(lines,"Upstream block","#%d" % int(state.upstream_block_id))
	if state.has("downstream_block_id"): field(lines,"Downstream block","#%d" % int(state.downstream_block_id))
	if state.has("signal_aspect"): field(lines,"Aspect",["GREEN","RED","RESERVED"][clampi(int(state.signal_aspect),0,2)])
	if state.has("rail_entity_id"): field(lines,"Current rail","#%d" % int(state.rail_entity_id))
	if state.has("travel_direction"): field(lines,"Travel direction",Format.direction(int(state.travel_direction)))
	if state.has("entry_direction"): field(lines,"Entry direction",Format.direction(int(state.entry_direction)))
	if state.has("movement_progress"): field(lines,"Movement","%d / %d ticks" % [int(state.movement_progress),int(state.get("movement_interval",0))])
	if state.has("next_rail_id"): field(lines,"Next rail","#%d" % int(state.next_rail_id))
	if state.has("locomotive_activity"): field(lines,"Locomotive activity",str(int(state.locomotive_activity)))
	if state.has("train_id"): field(lines,"Train ID","#%d" % int(state.train_id))
	if state.has("vehicle_count"): field(lines,"Vehicle count",str(int(state.vehicle_count)))
	if state.has("destination_station_id"): field(lines,"Destination","None" if int(state.destination_station_id) == 0 else "Station #%d" % int(state.destination_station_id))
	if state.has("route_status"): field(lines,"Route status",["NONE","ACTIVE","ARRIVED","INVALID"][clampi(int(state.route_status),0,3)])
	if state.has("route_length"): field(lines,"Route progress","%d / %d" % [int(state.get("route_index",0)),maxi(0,int(state.route_length)-1)])
	if state.has("next_planned_rail_id"): field(lines,"Next planned rail","#%d" % int(state.next_planned_rail_id))
	if state.has("current_block_id"): field(lines,"Current block","#%d" % int(state.current_block_id))
	if state.has("next_route_block_id"): field(lines,"Next route block","None" if int(state.next_route_block_id) == 0 else "#%d" % int(state.next_route_block_id))
	if state.has("reserved_block_id"): field(lines,"Reserved block","None" if int(state.reserved_block_id) == 0 else "#%d" % int(state.reserved_block_id))
	if state.has("reserved_blocks"): field(lines,"Reserved blocks",str(state.reserved_blocks))
	if state.has("reservation_status"): field(lines,"Reservation status",["NONE","HELD","WAITING","INVALID"][clampi(int(state.reservation_status),0,3)])
	if state.has("chain_status"): field(lines,"Chain status",["NONE","HELD","WAITING","INVALID"][clampi(int(state.chain_status),0,3)])
	if state.has("chain_required_block_count"): field(lines,"Chain blocks required",str(int(state.chain_required_block_count)))
	if state.has("schedule_enabled"):
		section(lines,"Schedule")
		field(lines,"Enabled",Format.yes_no(bool(state.schedule_enabled)))
		field(lines,"Stops",str(int(state.get("schedule_count",0))))
		field(lines,"Current stop",str(int(state.get("current_stop_index",0))+1))
		field(lines,"Scheduled station","None" if int(state.get("current_scheduled_station_id",0)) == 0 else "#%d" % int(state.current_scheduled_station_id))
		field(lines,"Schedule status",["DISABLED","TRAVELING","WAITING","ROUTE UNAVAILABLE"][clampi(int(state.get("schedule_status",0)),0,3)])
		field(lines,"Wait",["NONE","TIME","CARGO EMPTY","CARGO FULL"][clampi(int(state.get("wait_condition",0)),0,3)])
		if int(state.get("wait_condition",0)) == 1:
			field(lines,"Wait progress","%d / %d ticks" % [int(state.get("wait_progress",0)),int(state.get("wait_value",0))])
	if state.has("blocking_block_id"): field(lines,"Blocking block","None" if int(state.blocking_block_id) == 0 else "#%d" % int(state.blocking_block_id))
	if state.has("blocking_train_id"): field(lines,"Blocking train","None" if int(state.blocking_train_id) == 0 else "#%d" % int(state.blocking_train_id))
	if state.has("consist_index"): field(lines,"Consist index",str(int(state.consist_index)))
	if state.has("coupled"): field(lines,"Coupled",Format.yes_no(bool(state.coupled)))
	if state.has("recipe") or state.has("progress") or state.has("duration"):
		section(lines, "Process")
		if state.has("recipe"): field(lines, "Recipe ID", str(int(state.recipe)))
		if state.has("progress"): field(lines, "Progress", "%d / %d ticks" % [int(state.progress), int(state.get("duration", 0))])
	for key in ["processing", "generation_active", "conversion_active", "fuel_active"]:
		if state.has(key): field(lines, key.capitalize(), Format.yes_no(bool(state[key])))
	var inventory_keys := ["item", "quantity", "output_item", "output_quantity", "resource_remaining", "science_quantity", "science_capacity", "material_quantity", "supply_radius", "fluid_quantity", "fluid_capacity", "stored_water", "water_capacity", "stored_steam", "steam_capacity", "stored_exhaust", "exhaust_capacity", "stored_energy", "capacity", "stored_heat", "heat_capacity", "fuel_ticks", "energy_available", "cargo_item", "cargo_quantity", "cargo_capacity"]
	var has_inventory := false
	for key in inventory_keys:
		if state.has(key): has_inventory = true
	if has_inventory:
		section(lines, "Inventory and storage")
		if state.has("item"): field(lines, "Item", Format.item(int(state.item)))
		if state.has("fluid_type"): field(lines, "Fluid", Format.fluid(int(state.fluid_type)))
		for key in inventory_keys:
			if state.has(key) and key != "item": field(lines, key.capitalize(), Format.number(int(state[key])))
	if state.has("inventory"):
		section(lines, "Inventory and storage")
		var inventory: Array = state.inventory
		for index in inventory.size():
			if int(inventory[index]) != 0:
				field(lines, Format.item(index), Format.number(int(inventory[index])))
	if state.has("inputs"):
		section(lines, "Inputs")
		for slot: Dictionary in state.inputs:
			field(lines, Format.item(int(slot.get("item", 0))), Format.number(int(slot.get("quantity", 0))))
	if state.has("output") and state.output is Dictionary:
		var output: Dictionary = state.output
		field(lines, "Output: %s" % Format.item(int(output.get("item", 0))), Format.number(int(output.get("quantity", 0))))
	section(lines, "Activity")
	var activity_count := 0
	for key in state.keys():
		if str(key).ends_with("_last_tick"):
			field(lines, str(key).capitalize(), Format.number(int(state[key])))
			activity_count += 1
	if activity_count == 0: lines.pop_back()
	var telemetry: Dictionary = state.get("telemetry", {})
	if not telemetry.is_empty():
		section(lines, "Telemetry — last %d ticks" % int(telemetry.get("window_ticks", 0)))
		for key in ["received", "sent", "net_flow", "cycles", "working", "blocked_input", "blocked_output", "blocked", "unpowered", "idle", "depleted", "occupied", "holding", "pickups", "drops"]:
			var amount := int(telemetry.get(key, 0))
			if amount != 0: field(lines, key.capitalize(), Format.number(amount))
	details.text = "\n".join(lines)

func section(lines: Array[String], name: String) -> void:
	if not lines.is_empty(): lines.append("")
	lines.append("[b]%s[/b]" % name)

func field(lines: Array[String], name: String, value: String) -> void:
	lines.append("%s: %s" % [name, value])

func _hide_configuration() -> void:
	configuration_label.hide()
	configuration_selector.hide()
	secondary_configuration_label.hide()
	secondary_configuration_selector.hide()
	schedule_panel.hide()

func _show_configuration(type_id: int,state: Dictionary) -> void:
	configuring = true
	configuration_selector.clear()
	if type_id == 4:
		configuration_label.text = "Assembler recipe"
		for definition: Dictionary in recipe_catalog:
			var index := configuration_selector.item_count
			configuration_selector.add_item(str(definition.get("name","Recipe")))
			configuration_selector.set_item_metadata(index,int(definition.get("recipe_id",0)))
			configuration_selector.set_item_disabled(index,not bool(definition.get("unlocked",false)))
			if int(definition.get("recipe_id",0)) == int(state.get("recipe",0)):
				configuration_selector.select(index)
	elif type_id == 5:
		configuration_label.text = "Storage output item"
		for definition: Dictionary in item_catalog:
			var index := configuration_selector.item_count
			configuration_selector.add_item(str(definition.get("name","Item")))
			configuration_selector.set_item_metadata(index,int(definition.get("item_type",0)))
			if int(definition.get("item_type",0)) == int(state.get("configured_output",0)):
				configuration_selector.select(index)
	elif type_id == 26:
		configuration_label.text = "Rail switch branch"
		for branch in range(2):
			configuration_selector.add_item("Branch %s" % ("A" if branch == 0 else "B"))
			configuration_selector.set_item_metadata(branch,branch)
			if branch == int(state.get("selected_branch",0)):
				configuration_selector.select(branch)
	elif type_id == 25:
		configuration_label.text = "Freight mode"
		for mode in range(3):
			configuration_selector.add_item(["Disabled","Load","Unload"][mode])
			configuration_selector.set_item_metadata(mode,mode)
			if mode == int(state.get("freight_mode",0)):
				configuration_selector.select(mode)
		secondary_configuration_label.text = "Freight item"
		secondary_configuration_selector.clear()
		for definition: Dictionary in item_catalog:
			var index := secondary_configuration_selector.item_count
			secondary_configuration_selector.add_item(str(definition.get("name","Item")))
			secondary_configuration_selector.set_item_metadata(index,int(definition.get("item_type",0)))
			if int(definition.get("item_type",0)) == int(state.get("freight_item",0)):
				secondary_configuration_selector.select(index)
		secondary_configuration_label.show()
		secondary_configuration_selector.show()
	elif type_id == 27:
		configuration_label.text = "Train destination"
		configuration_selector.add_item("None")
		configuration_selector.set_item_metadata(0,0)
		for station: Dictionary in station_catalog:
			var index := configuration_selector.item_count
			var station_id := int(station.get("id",0))
			configuration_selector.add_item("Station #%d" % station_id)
			configuration_selector.set_item_metadata(index,station_id)
			if station_id == int(state.get("destination_station_id",0)):
				configuration_selector.select(index)
		schedule_panel.show()
	else:
		_hide_configuration()
		configuring = false
		return
	configuration_label.show()
	configuration_selector.show()
	configuring = false

func _on_configuration_selected(index: int) -> void:
	if configuring or index < 0: return
	var value := int(configuration_selector.get_item_metadata(index))
	if configuration_label.text == "Assembler recipe":
		assembler_recipe_requested.emit(entity_id,value)
	elif configuration_label.text == "Rail switch branch":
		rail_switch_branch_requested.emit(entity_id,value)
	elif configuration_label.text == "Train destination":
		train_destination_requested.emit(entity_id,value)
	elif configuration_label.text == "Freight mode":
		rail_station_freight_mode_requested.emit(entity_id,value)
	else:
		storage_output_requested.emit(entity_id,value)

func _on_secondary_configuration_selected(index: int) -> void:
	if configuring or index < 0: return
	var value := int(secondary_configuration_selector.get_item_metadata(index))
	rail_station_freight_item_requested.emit(entity_id,value)

func _on_schedule_enabled_toggled(enabled: bool) -> void:
	if configuring:
		return
	train_schedule_enabled_requested.emit(entity_id,enabled)

func _on_schedule_wait_selected(index: int) -> void:
	schedule_wait_ticks.visible = index == 1

func _on_schedule_add_pressed() -> void:
	if schedule_station_selector.selected < 0:
		return
	var station_id := int(schedule_station_selector.get_item_metadata(
		schedule_station_selector.selected))
	var wait_condition := schedule_wait_selector.selected
	var wait_value := int(schedule_wait_ticks.value) if wait_condition == 1 else 0
	train_schedule_add_requested.emit(
		entity_id,station_id,wait_condition,wait_value)

func _on_schedule_remove_pressed() -> void:
	if schedule_stop_selector.selected >= 0:
		train_schedule_remove_requested.emit(entity_id,schedule_stop_selector.selected)

func _on_schedule_clear_pressed() -> void:
	train_schedule_clear_requested.emit(entity_id)
