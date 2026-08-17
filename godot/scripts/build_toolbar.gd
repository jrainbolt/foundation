class_name FoundationBuildToolbar
extends HBoxContainer

signal build_selected(entity_type: int)
signal demolish_selected
const Format := preload("res://scripts/presentation_format.gd")
const CATEGORY_ORDER := ["Logistics","Production","Power","Fluids & Heat","Rail","Infrastructure"]
const CATEGORY_TYPES := {
	"Logistics":[2,5,6,7], "Production":[1,3,4],
	"Power":[8,9,15,16], "Fluids & Heat":[10,11,12,13,14,17,18,19,20,21],
	"Rail":[24,25,26,27,28,29,30], "Infrastructure":[22,23]
}
var buttons: Dictionary = {}
var category_buttons: Dictionary = {}
var definitions: Dictionary = {}
var selected_category := "Logistics"
var item_row: HBoxContainer
var info_label: Label

func configure(simulation: Object) -> void:
	if not buttons.is_empty():
		refresh(simulation)
		return
	var categories := VBoxContainer.new()
	categories.custom_minimum_size.x = 132
	add_child(categories)
	for category: String in CATEGORY_ORDER:
		var category_button := Button.new()
		category_button.text = category
		category_button.toggle_mode = true
		category_button.pressed.connect(func() -> void: _select_category(category))
		category_buttons[category] = category_button
		categories.add_child(category_button)
	var content := VBoxContainer.new()
	content.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	add_child(content)
	var scroll := ScrollContainer.new()
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_AUTO
	scroll.vertical_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	scroll.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	content.add_child(scroll)
	item_row = HBoxContainer.new()
	item_row.add_theme_constant_override("separation",8)
	scroll.add_child(item_row)
	info_label = Label.new()
	info_label.name = "BuildInfoLabel"
	info_label.text = "Choose a build category. Costs are validated when the command executes."
	info_label.add_theme_font_size_override("font_size",12)
	content.add_child(info_label)
	for definition: Dictionary in simulation.get_build_catalog():
		var entity_type := int(definition.entity_type)
		definitions[entity_type] = definition
		var button := Button.new()
		button.toggle_mode = true
		button.custom_minimum_size = Vector2(122,48)
		button.add_theme_font_size_override("font_size",12)
		_apply_button_styles(button)
		button.pressed.connect(func() -> void:
			_show_definition(entity_type)
			build_selected.emit(entity_type))
		buttons[entity_type] = button
		item_row.add_child(button)
	var demolish := Button.new()
	demolish.name = "DemolishButton"
	demolish.text = "Demolish"
	demolish.toggle_mode = true
	demolish.custom_minimum_size = Vector2(112,48)
	_apply_button_styles(demolish)
	demolish.pressed.connect(func() -> void: demolish_selected.emit())
	item_row.add_child(demolish)
	_select_category(selected_category)
	refresh(simulation)

func refresh(simulation: Object) -> void:
	for definition: Dictionary in simulation.get_build_catalog():
		definitions[int(definition.entity_type)] = definition
	for entity_type: int in buttons:
		var button: Button = buttons[entity_type]
		var definition: Dictionary = definitions.get(entity_type,{})
		var unlocked := bool(definition.get("unlocked",false))
		button.disabled = not unlocked
		button.text = "%s\n%d units%s" % [Format.entity_type(entity_type),
			int(definition.get("construction_cost",0)),"" if unlocked else "  • LOCKED"]

func _select_category(category: String) -> void:
	selected_category = category
	for name: String in category_buttons:
		category_buttons[name].button_pressed = name == category
	var visible_types: Array = CATEGORY_TYPES.get(category,[])
	for entity_type: int in buttons:
		buttons[entity_type].visible = entity_type in visible_types
	_show_category_summary()

func _show_category_summary() -> void:
	if info_label != null:
		info_label.text = "%s — locked entries stay visible so the technology path remains clear." % selected_category

func _show_definition(entity_type: int) -> void:
	var definition: Dictionary = definitions.get(entity_type,{})
	info_label.text = "%s  •  Cost %d construction units  •  %s" % [
		Format.entity_type(entity_type),int(definition.get("construction_cost",0)),
		"Available" if bool(definition.get("unlocked",false)) else "Requires research"]

func set_active(entity_type: int, demolishing: bool) -> void:
	for value: int in buttons:
		buttons[value].button_pressed = value == entity_type and not demolishing
	var demolish: Button = get_node_or_null("DemolishButton")
	if demolish == null and item_row != null: demolish = item_row.get_node_or_null("DemolishButton")
	if demolish != null: demolish.button_pressed = demolishing

func _apply_button_styles(button: Button) -> void:
	button.add_theme_stylebox_override("normal",_button_style("#242c37","#465363",1))
	button.add_theme_stylebox_override("hover",_button_style("#303b48","#dce8f2",2))
	button.add_theme_stylebox_override("pressed",_button_style("#51491f","#f4d65e",3))
	button.add_theme_stylebox_override("disabled",_button_style("#1a2028","#333c47",1))
	button.add_theme_color_override("font_color",Color("#ecf1f5"))
	button.add_theme_color_override("font_disabled_color",Color("#68727d"))

func _button_style(fill: String,border: String,width: int) -> StyleBoxFlat:
	var style := StyleBoxFlat.new()
	style.bg_color=Color(fill); style.border_color=Color(border)
	style.set_border_width_all(width); style.set_corner_radius_all(5)
	style.content_margin_left=10; style.content_margin_right=10
	style.content_margin_top=6; style.content_margin_bottom=6
	return style
