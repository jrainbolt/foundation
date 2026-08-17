class_name FoundationResearchPanel
extends PanelContainer

signal research_requested(technology_id: int)
const Format := preload("res://scripts/presentation_format.gd")
var cards: Dictionary = {}
var list: VBoxContainer

func _ready() -> void:
	list = VBoxContainer.new()
	list.add_theme_constant_override("separation",8)
	add_child(list)

func refresh(catalog: Array) -> void:
	if list == null: return
	for child: Node in list.get_children(): child.queue_free()
	cards.clear()
	var title := Label.new()
	title.text = "RESEARCH & PROGRESSION"
	title.add_theme_font_size_override("font_size",18)
	list.add_child(title)
	for technology: Dictionary in catalog:
		var technology_id := int(technology.technology_id)
		var card := Button.new()
		card.alignment = HORIZONTAL_ALIGNMENT_LEFT
		card.custom_minimum_size = Vector2(360,76)
		var state := "COMPLETED" if bool(technology.completed) else (
			"RESEARCHING" if bool(technology.active) else (
			"AVAILABLE" if bool(technology.available) else "PREREQUISITE LOCKED"))
		var prerequisites: Array[String] = []
		for prerequisite: int in technology.prerequisites:
			prerequisites.append(Format.technology(prerequisite))
		var unlocks: Array[String] = []
		for entity_type: int in technology.unlocked_entities:
			unlocks.append(Format.entity_type(entity_type))
		card.text = "%s   [%s]\nScience: %d × %s   Progress: %d / %d\nPrerequisite: %s   Unlocks: %s" % [
			Format.technology(technology_id),state,
			int(technology.science_quantity_per_unit),Format.item(int(technology.science_item)),
			int(technology.completed_units),int(technology.required_units),
			"None" if prerequisites.is_empty() else ", ".join(prerequisites),
			"content" if unlocks.is_empty() else ", ".join(unlocks)]
		card.disabled = not bool(technology.available) or bool(technology.active)
		card.pressed.connect(func() -> void: research_requested.emit(technology_id))
		cards[technology_id] = card
		list.add_child(card)

