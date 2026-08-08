class_name FoundationBuildToolbar
extends HBoxContainer

signal build_selected(entity_type: int)
signal demolish_selected
const Format := preload("res://scripts/presentation_format.gd")
const CURATED_TYPES := [1,2,7,5,4,8,9,11,12,13,14,21]
var buttons: Dictionary = {}

func configure(simulation: Object) -> void:
	if buttons.is_empty():
		for entity_type: int in CURATED_TYPES:
			var button := Button.new()
			button.toggle_mode = true
			button.custom_minimum_size = Vector2(86, 42)
			button.pressed.connect(func() -> void: build_selected.emit(entity_type))
			buttons[entity_type] = button
			add_child(button)
		var demolish := Button.new()
		demolish.name = "DemolishButton"
		demolish.text = "Demolish"
		demolish.toggle_mode = true
		demolish.custom_minimum_size = Vector2(94, 42)
		demolish.pressed.connect(func() -> void: demolish_selected.emit())
		add_child(demolish)
	refresh(simulation)

func refresh(simulation: Object) -> void:
	var catalog_by_type := {}
	for definition: Dictionary in simulation.get_build_catalog():
		catalog_by_type[int(definition.entity_type)] = definition
	for entity_type: int in buttons:
		var button: Button = buttons[entity_type]
		var definition: Dictionary = catalog_by_type.get(entity_type, {})
		var unlocked := bool(definition.get("unlocked", false))
		button.disabled = not unlocked
		button.text = "%s\n%d units%s" % [
			Format.entity_type(entity_type),
			int(definition.get("construction_cost", 0)),
			"" if unlocked else "  LOCKED"
		]

func set_active(entity_type: int, demolishing: bool) -> void:
	for value: int in buttons:
		buttons[value].button_pressed = value == entity_type and not demolishing
	var demolish: Button = get_node_or_null("DemolishButton")
	if demolish != null: demolish.button_pressed = demolishing
