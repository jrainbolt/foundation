extends SceneTree

func _initialize() -> void:
	call_deferred("_capture")

func _capture() -> void:
	var requested := OS.get_environment("FOUNDATION_CAPTURE_SIZE")
	if requested == "2560x1440": root.size = Vector2i(2560,1440)
	elif requested == "3840x2160": root.size = Vector2i(3840,2160)
	var packed: PackedScene = load("res://scenes/main.tscn")
	if packed == null:
		push_error("main scene did not load")
		quit(1)
		return
	var main: Control = packed.instantiate()
	root.add_child(main)
	await process_frame
	for unused in range(40):
		if not main._advance(1):
			push_error("visual capture could not advance demo")
			quit(1)
			return
	if OS.get_environment("FOUNDATION_CAPTURE_PANEL") == "research":
		main._toggle_research()
	for unused in range(3):
		await process_frame
	var image: Image = root.get_texture().get_image()
	var result := image.save_png("/tmp/foundation-godot-visual.png")
	if result != OK:
		push_error("visual capture failed: %d" % result)
		quit(1)
		return
	main.queue_free()
	await process_frame
	print("Foundation visual capture written")
	quit(0)
