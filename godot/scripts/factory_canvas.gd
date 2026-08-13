class_name FoundationFactoryCanvas
extends Node2D

const EntityVisual := preload("res://scripts/entity_visual.gd")
const CELL := 76.0

var entity_nodes: Dictionary = {}
var resources: Array = []
var edges: Array = []
var terrain: Array = []
var selected_route: Array = []
var selected_current_block := 0
var selected_reserved_block := 0
var selected_blocked_block := 0

func set_selected_route(route: Array) -> void:
	selected_route = route.duplicate(true)
	queue_redraw()

func set_selected_train_blocks(current_block: int,reserved_block: int,blocked_block: int) -> void:
	selected_current_block = current_block
	selected_reserved_block = reserved_block
	selected_blocked_block = blocked_block
	queue_redraw()

func terrain_is_buildable(grid: Vector2i) -> bool:
	for cell: Dictionary in terrain:
		if int(cell.x) == grid.x and int(cell.y) == grid.y:
			return bool(cell.buildable)
	return false

func synchronize(
	entities: Array, next_resources: Array, next_edges: Array,
	next_terrain: Array
	) -> void:
	var live_ids := {}
	var resource_by_entity := {}
	for resource: Dictionary in next_resources:
		var occupying_id := int(resource.occupying_entity_id)
		if occupying_id != 0:
			resource_by_entity[occupying_id] = resource
	for source_entity: Dictionary in entities:
		var entity: Dictionary = source_entity.duplicate(true)
		var entity_id := int(entity.id)
		if resource_by_entity.has(entity_id):
			var resource: Dictionary = resource_by_entity[entity_id]
			entity["resource_type"] = int(resource.type)
			entity["resource_remaining"] = int(resource.remaining)
		live_ids[entity_id] = true
		var visual: FoundationEntityVisual = entity_nodes.get(entity_id)
		if visual == null:
			visual = EntityVisual.new()
			entity_nodes[entity_id] = visual
			add_child(visual)
		visual.apply(entity)
	for entity_id: int in entity_nodes.keys():
		if not live_ids.has(entity_id):
			entity_nodes[entity_id].queue_free()
			entity_nodes.erase(entity_id)
	resources = next_resources.duplicate(true)
	edges = next_edges.duplicate(true)
	terrain = next_terrain.duplicate(true)
	queue_redraw()

func _draw() -> void:
	for cell: Dictionary in terrain:
		var terrain_type := int(cell.type)
		var color := Color("#243229")
		if terrain_type == 2: color = Color("#18394a")
		elif terrain_type == 3: color = Color("#353941")
		draw_rect(Rect2(float(cell.x)*CELL,float(cell.y)*CELL,CELL,CELL),color)
	for resource: Dictionary in resources:
		var color := Color("#984f35") if int(resource.type) == 1 else Color("#b87333")
		var remaining := int(resource.remaining)
		var radius := 28.0 if remaining >= 100 else (22.0 if remaining > 0 else 18.0)
		if bool(resource.get("depleted", false)): color = Color("#39414a")
		var center := Vector2(
			(float(resource.x) + 0.5) * CELL,
			(float(resource.y) + 0.5) * CELL
		)
		draw_circle(center, radius, color.darkened(0.45))
		if remaining == 0:
			draw_line(center + Vector2(-10,-10),center + Vector2(10,10),Color("#9ba4ad"),3.0)
			draw_line(center + Vector2(10,-10),center + Vector2(-10,10),Color("#9ba4ad"),3.0)
		draw_string(
			ThemeDB.fallback_font, center + Vector2(-28, 4),
			"%d" % remaining, HORIZONTAL_ALIGNMENT_CENTER, 56, 11,
			Color("#f2e6dd")
		)
	for edge: Dictionary in edges:
		var a: FoundationEntityVisual = entity_nodes.get(int(edge.a))
		var b: FoundationEntityVisual = entity_nodes.get(int(edge.b))
		if a != null and b != null:
				draw_line(
				a.position + Vector2(38, 38),
				b.position + Vector2(38, 38),
				Color("#f2d95c", 0.9), 4.0
				)
	for entity_id: int in entity_nodes.keys():
		var visual: FoundationEntityVisual = entity_nodes[entity_id]
		var block_id := int(visual.state.get("block_id",0))
		var highlight := Color.TRANSPARENT
		if block_id != 0 and block_id == selected_blocked_block:
			highlight = Color("#ff785c",0.78)
		elif block_id != 0 and block_id == selected_reserved_block:
			highlight = Color("#55d7ff",0.72)
		elif block_id != 0 and block_id == selected_current_block:
			highlight = Color("#62d58b",0.62)
		if highlight.a > 0.0:
			draw_rect(Rect2(visual.position + Vector2(4,4),Vector2(68,68)),highlight,false,5.0)
	if selected_route.size() > 1:
		for index in range(selected_route.size() - 1):
			var a_id := int(selected_route[index].rail_entity_id)
			var b_id := int(selected_route[index + 1].rail_entity_id)
			var a: FoundationEntityVisual = entity_nodes.get(a_id)
			var b: FoundationEntityVisual = entity_nodes.get(b_id)
			if a != null and b != null:
				draw_line(a.position + Vector2(38,38),b.position + Vector2(38,38),
					Color("#72e6ff",0.9),5.0)
