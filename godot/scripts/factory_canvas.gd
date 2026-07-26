class_name FoundationFactoryCanvas
extends Node2D

const EntityVisual := preload("res://scripts/entity_visual.gd")
const CELL := 64.0

var entity_nodes: Dictionary = {}
var resources: Array = []
var edges: Array = []

func synchronize(
	entities: Array, next_resources: Array, next_edges: Array
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
	queue_redraw()

func _draw() -> void:
	for x in range(13):
		draw_line(Vector2(x * CELL, 0), Vector2(x * CELL, 8 * CELL), Color("#303846"))
	for y in range(9):
		draw_line(Vector2(0, y * CELL), Vector2(12 * CELL, y * CELL), Color("#303846"))
	for resource: Dictionary in resources:
		var color := Color("#984f35") if int(resource.type) == 1 else Color("#b87333")
		var center := Vector2(
			(float(resource.x) + 0.5) * CELL,
			(float(resource.y) + 0.5) * CELL
		)
		draw_circle(center, 25.0, color.darkened(0.35))
		draw_string(
			ThemeDB.fallback_font, center + Vector2(-25, 4),
			"%d" % int(resource.remaining), HORIZONTAL_ALIGNMENT_CENTER, 50, 11
		)
	for edge: Dictionary in edges:
		var a: FoundationEntityVisual = entity_nodes.get(int(edge.a))
		var b: FoundationEntityVisual = entity_nodes.get(int(edge.b))
		if a != null and b != null:
			draw_line(
				a.position + Vector2(32, 32),
				b.position + Vector2(32, 32),
				Color("#f1d55c", 0.75), 3.0
			)
