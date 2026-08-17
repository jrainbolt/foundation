extends SceneTree

func _fail(message: String) -> void:
	push_error(message)
	quit(1)

func _initialize() -> void:
	call_deferred("_run")

func _run() -> void:
	var scene: PackedScene = load("res://scenes/main.tscn")
	var main := scene.instantiate()
	root.add_child(main)
	await process_frame
	await process_frame
	if not main.tick_label.text.contains("Day:") \
			or not main.construction_label.text.contains("supply"):
		_fail("compact HUD does not expose time and physical depot supply semantics: %s / %s" % [main.tick_label.text,main.construction_label.text])
		return
	if main.selection_summary.text != "Selected: None":
		_fail("compact selection summary has an unsafe initial state")
		return
	var technologies: Array = main.simulation.get_technology_catalog()
	if technologies.size() != 3 or main.research_panel.cards.size() != 3:
		_fail("research panel is not derived from the immutable technology catalog")
		return
	var advanced: Dictionary = technologies[2]
	if not bool(advanced.available) or advanced.unlocked_entities.is_empty():
		_fail("advanced technology availability/unlock consequences are missing")
		return
	if not main.build_toolbar.category_buttons.has("Rail") \
			or not main.build_toolbar.buttons.has(30) \
			or not main.build_toolbar.buttons[30].disabled:
		_fail("categorized build menu must retain visibly locked content")
		return
	main._toggle_history()
	if not main.event_log.visible:
		_fail("secondary event history did not expand")
		return
	main._toggle_history()
	main._toggle_research()
	if not main.research_panel.visible:
		_fail("research panel did not open")
		return
	var before_tick := int(main.simulation.get_tick())
	main._on_research_requested(3)
	if int(main.simulation.get_tick()) != before_tick + 1 \
			or int(main.simulation.get_research().active_technology_id) != 3:
		_fail("research selection did not use the normal FIFO command/tick path")
		return
	main._append_events([{"tick":before_tick,"type":38,"entity_id":0,
		"related_entity_id":0,"item_type":0,"quantity":3,"technology_id":3}])
	if not main.notification_label.text.contains("Advanced Manufacturing"):
		_fail("important committed event notification is missing")
		return
	main._reset_demo()
	if main.research_panel.visible or main.alert_panel.visible \
			or not main.notification_label.text.is_empty():
		_fail("reset did not clear transient HUD state")
		return
	print("Foundation gameplay HUD smoke test passed")
	quit(0)
