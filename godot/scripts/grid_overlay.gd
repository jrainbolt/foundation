class_name FoundationGridOverlay
extends Node2D

@export var cell_size := 76.0
@export var grid_size := Vector2i(13, 9)
@export var margin_cells := 2

func _ready() -> void:
	z_index = -20
	queue_redraw()

func _draw() -> void:
	var left := -margin_cells
	var top := -margin_cells
	var right := grid_size.x + margin_cells
	var bottom := grid_size.y + margin_cells
	for x in range(left, right + 1):
		draw_line(Vector2(x * cell_size, top * cell_size), Vector2(x * cell_size, bottom * cell_size), Color("#242b35", 0.48), 0.75)
	for y in range(top, bottom + 1):
		draw_line(Vector2(left * cell_size, y * cell_size), Vector2(right * cell_size, y * cell_size), Color("#242b35", 0.48), 0.75)
