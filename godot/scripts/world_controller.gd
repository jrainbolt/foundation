class_name FoundationWorldController
extends Node2D

signal selected_entity_changed(entity_id: int)
signal hovered_grid_changed(grid_position: Vector2i)
const CELL := 64.0
@export var canvas_path: NodePath
@export var inspector_path: NodePath
@onready var canvas: FoundationFactoryCanvas = get_node(canvas_path)
@onready var inspector: Control = get_node(inspector_path)
var selected_entity_id := 0
var hovered_grid := Vector2i.ZERO

func world_to_grid(world_position: Vector2) -> Vector2i:
	return Vector2i(floori(world_position.x / CELL), floori(world_position.y / CELL))

func grid_to_world(grid_position: Vector2i) -> Vector2:
	return Vector2(grid_position) * CELL

func select_entity(entity_id: int) -> bool:
	if entity_id != 0 and not canvas.entity_nodes.has(entity_id):
		clear_selection()
		return false
	if selected_entity_id != 0 and canvas.entity_nodes.has(selected_entity_id):
		canvas.entity_nodes[selected_entity_id].set_selected(false)
	selected_entity_id = entity_id
	if entity_id == 0:
		inspector.clear_entity()
	else:
		var visual: FoundationEntityVisual = canvas.entity_nodes[entity_id]
		visual.set_selected(true)
		inspector.show_entity(visual.state)
	selected_entity_changed.emit(selected_entity_id)
	return entity_id != 0

func clear_selection() -> void:
	select_entity(0)

func refresh_selection() -> void:
	if selected_entity_id == 0:
		inspector.clear_entity()
	elif canvas.entity_nodes.has(selected_entity_id):
		var visual: FoundationEntityVisual = canvas.entity_nodes[selected_entity_id]
		visual.set_selected(true)
		inspector.show_entity(visual.state)
	else:
		clear_selection()

func pick_grid(grid_position: Vector2i) -> int:
	var matches: Array[int] = []
	for entity_id: int in canvas.entity_nodes:
		var visual: FoundationEntityVisual = canvas.entity_nodes[entity_id]
		if Vector2i(int(visual.state.x), int(visual.state.y)) == grid_position:
			matches.append(entity_id)
	matches.sort()
	return matches[0] if not matches.is_empty() else 0

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseMotion:
		var next_grid := world_to_grid(get_global_mouse_position())
		if next_grid != hovered_grid:
			hovered_grid = next_grid
			hovered_grid_changed.emit(hovered_grid)
			queue_redraw()
	elif event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT and event.pressed:
		select_entity(pick_grid(world_to_grid(get_global_mouse_position())))
		get_viewport().set_input_as_handled()

func _draw() -> void:
	draw_rect(Rect2(grid_to_world(hovered_grid), Vector2(CELL, CELL)), Color("#78c7ff", 0.10), true)
	draw_rect(Rect2(grid_to_world(hovered_grid), Vector2(CELL, CELL)), Color("#78c7ff", 0.75), false, 2.0)
