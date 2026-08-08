class_name FoundationEntityInspector
extends PanelContainer

const Format := preload("res://scripts/presentation_format.gd")
@onready var title_label: Label = %InspectorTitle
@onready var details: RichTextLabel = %InspectorDetails
var entity_id := 0

func clear_entity() -> void:
	entity_id = 0
	title_label.text = "Entity Inspector"
	details.text = "Select an entity to inspect it."

func show_entity(state: Dictionary) -> void:
	if state.is_empty():
		clear_entity()
		return
	entity_id = int(state.get("id", 0))
	var type_id := int(state.get("type", 0))
	title_label.text = Format.entity_type(type_id)
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
