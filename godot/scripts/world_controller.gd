class_name FoundationWorldController
extends Node2D

signal selected_entity_changed(entity_id: int)
signal hovered_grid_changed(grid_position: Vector2i)
signal placement_requested(entity_type: int,grid_position: Vector2i,direction: int)
signal demolition_requested(entity_id: int)
signal interaction_mode_changed(mode: int,entity_type: int)
enum InteractionMode { SELECT, BUILD, DEMOLISH }
const CELL := 76.0
@export var canvas_path: NodePath
@export var inspector_path: NodePath
@onready var canvas: FoundationFactoryCanvas = get_node(canvas_path)
@onready var inspector: Control = get_node(inspector_path)
var selected_entity_id := 0
var hovered_grid := Vector2i.ZERO
var mode := InteractionMode.SELECT
var build_entity_type := 0
var build_direction := 0
var hovered_entity_id := 0

func enter_select_mode() -> void:
	mode = InteractionMode.SELECT
	build_entity_type = 0
	queue_redraw()
	interaction_mode_changed.emit(mode,build_entity_type)

func enter_build_mode(entity_type: int) -> void:
	mode = InteractionMode.BUILD
	build_entity_type = entity_type
	build_direction = 0
	queue_redraw()
	interaction_mode_changed.emit(mode,build_entity_type)

func enter_demolish_mode() -> void:
	mode = InteractionMode.DEMOLISH
	build_entity_type = 0
	queue_redraw()
	interaction_mode_changed.emit(mode,build_entity_type)

func rotate_build() -> void:
	if mode == InteractionMode.BUILD:
		build_direction = (build_direction + 1) % 4
		queue_redraw()

func preview_is_advisably_valid() -> bool:
	return mode == InteractionMode.BUILD and pick_grid(hovered_grid) == 0

func set_hovered_grid(grid_position: Vector2i) -> void:
	hovered_grid = grid_position
	var next_entity := pick_grid(hovered_grid)
	if hovered_entity_id != next_entity:
		if canvas.entity_nodes.has(hovered_entity_id):
			canvas.entity_nodes[hovered_entity_id].set_hovered(false)
		hovered_entity_id = next_entity
		if canvas.entity_nodes.has(hovered_entity_id):
			canvas.entity_nodes[hovered_entity_id].set_hovered(true)
	hovered_grid_changed.emit(hovered_grid)
	queue_redraw()

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
	if event.is_action_pressed("ui_cancel"):
		enter_select_mode()
		get_viewport().set_input_as_handled()
	elif event.is_action_pressed("rotate_build"):
		rotate_build()
		get_viewport().set_input_as_handled()
	elif event is InputEventMouseMotion:
		var next_grid := world_to_grid(get_global_mouse_position())
		if next_grid != hovered_grid:
			set_hovered_grid(next_grid)
	elif event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT and event.pressed:
		var grid := world_to_grid(get_global_mouse_position())
		var picked := pick_grid(grid)
		if mode == InteractionMode.BUILD:
			placement_requested.emit(build_entity_type,grid,build_direction)
		elif mode == InteractionMode.DEMOLISH:
			if picked != 0: demolition_requested.emit(picked)
		else:
			select_entity(picked)
		get_viewport().set_input_as_handled()

func _draw() -> void:
	draw_rect(Rect2(grid_to_world(hovered_grid), Vector2(CELL, CELL)), Color("#78c7ff", 0.10), true)
	draw_rect(Rect2(grid_to_world(hovered_grid), Vector2(CELL, CELL)), Color("#78c7ff", 0.75), false, 2.0)
	if mode == InteractionMode.BUILD:
		var preview := Rect2(grid_to_world(hovered_grid)+Vector2(5,5),Vector2(CELL-10,CELL-10))
		var color := Color("#62d98b",0.42) if preview_is_advisably_valid() else Color("#ef6262",0.42)
		draw_rect(preview,color,true)
		draw_rect(preview,color.lightened(0.35),false,3.0)
		draw_string(ThemeDB.fallback_font,preview.position+Vector2(5,18),"BUILD %d" % build_entity_type,HORIZONTAL_ALIGNMENT_LEFT,-1,11)
		var vectors := [Vector2.UP,Vector2.RIGHT,Vector2.DOWN,Vector2.LEFT]
		var center := preview.get_center()
		draw_line(center,center+vectors[build_direction]*20.0,Color.WHITE,3.0)
	elif mode == InteractionMode.DEMOLISH:
		draw_line(grid_to_world(hovered_grid)+Vector2(12,12),grid_to_world(hovered_grid)+Vector2(52,52),Color("#ff6868"),5.0)
		draw_line(grid_to_world(hovered_grid)+Vector2(52,12),grid_to_world(hovered_grid)+Vector2(12,52),Color("#ff6868"),5.0)
