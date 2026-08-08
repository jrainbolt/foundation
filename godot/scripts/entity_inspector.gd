class_name FoundationEntityInspector
extends PanelContainer

signal assembler_recipe_requested(entity_id: int,recipe_id: int)
signal storage_output_requested(entity_id: int,item_type: int)

const Format := preload("res://scripts/presentation_format.gd")
@onready var title_label: Label = %InspectorTitle
@onready var details: RichTextLabel = %InspectorDetails
@onready var configuration_label: Label = %ConfigurationLabel
@onready var configuration_selector: OptionButton = %ConfigurationSelector
var entity_id := 0
var recipe_catalog: Array = []
var item_catalog: Array = []
var configuring := false

func _ready() -> void:
	configuration_selector.item_selected.connect(_on_configuration_selected)

func configure_catalogs(recipes: Array,items: Array) -> void:
	recipe_catalog = recipes.duplicate(true)
	item_catalog = items.duplicate(true)

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
	if state.has("recipe") or state.has("progress") or state.has("duration"):
		section(lines, "Process")
		if state.has("recipe"): field(lines, "Recipe ID", str(int(state.recipe)))
		if state.has("progress"): field(lines, "Progress", "%d / %d ticks" % [int(state.progress), int(state.get("duration", 0))])
	for key in ["processing", "generation_active", "conversion_active", "fuel_active"]:
		if state.has(key): field(lines, key.capitalize(), Format.yes_no(bool(state[key])))
	var inventory_keys := ["item", "quantity", "output_item", "output_quantity", "fluid_quantity", "fluid_capacity", "stored_water", "water_capacity", "stored_steam", "steam_capacity", "stored_exhaust", "exhaust_capacity", "stored_energy", "capacity", "stored_heat", "heat_capacity", "fuel_ticks", "energy_available"]
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
	details.text = "\n".join(lines)

func section(lines: Array[String], name: String) -> void:
	if not lines.is_empty(): lines.append("")
	lines.append("[b]%s[/b]" % name)

func field(lines: Array[String], name: String, value: String) -> void:
	lines.append("%s: %s" % [name, value])

func _hide_configuration() -> void:
	configuration_label.hide()
	configuration_selector.hide()

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
	else:
		storage_output_requested.emit(entity_id,value)
