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
			button.custom_minimum_size = Vector2(112, 54)
			button.add_theme_font_size_override("font_size", 13)
			_apply_button_styles(button)
			button.pressed.connect(func() -> void: build_selected.emit(entity_type))
			buttons[entity_type] = button
			add_child(button)
		var demolish := Button.new()
		demolish.name = "DemolishButton"
		demolish.text = "Demolish"
		demolish.toggle_mode = true
		demolish.custom_minimum_size = Vector2(112, 54)
		demolish.add_theme_font_size_override("font_size", 13)
		_apply_button_styles(demolish)
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

func _apply_button_styles(button: Button) -> void:
	button.add_theme_stylebox_override("normal", _button_style("#242c37", "#465363", 1))
	button.add_theme_stylebox_override("hover", _button_style("#303b48", "#dce8f2", 2))
	button.add_theme_stylebox_override("pressed", _button_style("#51491f", "#f4d65e", 3))
	button.add_theme_stylebox_override("disabled", _button_style("#1a2028", "#333c47", 1))
	button.add_theme_color_override("font_color", Color("#ecf1f5"))
	button.add_theme_color_override("font_hover_color", Color.WHITE)
	button.add_theme_color_override("font_pressed_color", Color("#fff3b0"))
	button.add_theme_color_override("font_disabled_color", Color("#68727d"))

func _button_style(fill: String, border: String, width: int) -> StyleBoxFlat:
	var style := StyleBoxFlat.new()
	style.bg_color = Color(fill)
	style.border_color = Color(border)
	style.set_border_width_all(width)
	style.set_corner_radius_all(5)
	style.content_margin_left = 12
	style.content_margin_right = 12
	style.content_margin_top = 7
	style.content_margin_bottom = 7
	return style
