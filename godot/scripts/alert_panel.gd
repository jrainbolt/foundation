class_name FoundationAlertPanel
extends PanelContainer

signal entity_requested(entity_id: int)
var list: VBoxContainer
var alert_count := 0

func _ready() -> void:
	list = VBoxContainer.new()
	add_child(list)

func refresh(groups: Dictionary) -> void:
	if list == null: return
	for child: Node in list.get_children(): child.queue_free()
	alert_count = 0
	var title := Label.new(); title.text = "ACTIVE ALERTS"; list.add_child(title)
	for label: String in groups:
		var ids: Array = groups[label]
		if ids.is_empty(): continue
		alert_count += ids.size()
		var button := Button.new()
		button.text = "%s  × %d" % [label,ids.size()]
		button.alignment = HORIZONTAL_ALIGNMENT_LEFT
		button.tooltip_text = "Select and center the first affected entity"
		button.pressed.connect(func() -> void: entity_requested.emit(int(ids[0])))
		list.add_child(button)
	if alert_count == 0:
		var empty := Label.new(); empty.text = "No reliable active alerts"; list.add_child(empty)
