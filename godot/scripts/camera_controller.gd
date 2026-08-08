class_name FoundationCameraController
extends Camera2D

@export var movement_speed := 520.0
@export var minimum_zoom := 0.45
@export var maximum_zoom := 2.25
@export var zoom_step := 1.15
var dragging := false
var drag_distance := 0.0

func _ready() -> void:
	position = Vector2(494, 304)
	zoom = Vector2(0.72, 0.72)

func _process(delta: float) -> void:
	var axis := Input.get_vector("camera_left", "camera_right", "camera_up", "camera_down")
	if axis != Vector2.ZERO:
		position += axis * movement_speed * delta / zoom.x

func set_zoom_level(value: float) -> void:
	var bounded := clampf(value, minimum_zoom, maximum_zoom)
	zoom = Vector2(bounded, bounded)

func zoom_by(factor: float) -> void:
	set_zoom_level(zoom.x * factor)

func move_by(delta_position: Vector2) -> void:
	position += delta_position

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_MIDDLE:
		dragging = event.pressed
		drag_distance = 0.0
		get_viewport().set_input_as_handled()
	elif event is InputEventMouseMotion and dragging:
		position -= event.relative / zoom.x
		drag_distance += event.relative.length()
		get_viewport().set_input_as_handled()
	elif event.is_action_pressed("camera_zoom_in"):
		zoom_by(zoom_step)
		get_viewport().set_input_as_handled()
	elif event.is_action_pressed("camera_zoom_out"):
		zoom_by(1.0 / zoom_step)
		get_viewport().set_input_as_handled()
