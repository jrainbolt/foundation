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
var selected_reserved_blocks: Array = []
var selected_blocked_block := 0
var cosmetic_time := 0.0
var redraw_accumulator := 0.0

func _process(delta: float) -> void:
	cosmetic_time = fmod(cosmetic_time + delta, 1000.0)
	redraw_accumulator += delta
	if redraw_accumulator >= 0.10:
		redraw_accumulator = 0.0
		queue_redraw()

func decoration_signature(grid: Vector2i) -> int:
	# Stable coordinate-derived cosmetic seed. It is never written to Foundation.
	return absi((grid.x * 73856093) ^ (grid.y * 19349663))

func set_selected_route(route: Array) -> void:
	selected_route = route.duplicate(true)
	queue_redraw()

func set_selected_train_blocks(current_block: int,reserved_blocks: Array,blocked_block: int) -> void:
	selected_current_block = current_block
	selected_reserved_blocks = reserved_blocks.duplicate()
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
		visual.z_index = 20 if int(entity.type) in [27,28] else (10 if int(entity.type) not in [24,26,29,30] else 4)
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
		var grid := Vector2i(int(cell.x),int(cell.y))
		var seed := decoration_signature(grid)
		var origin := Vector2(grid) * CELL
		var variation := float(seed % 9) / 100.0
		var color := Color("#26392d").lightened(variation)
		if terrain_type == 2: color = Color("#123849").lightened(variation * 0.65)
		elif terrain_type == 3: color = Color("#34373b").lightened(variation * 0.45)
		draw_rect(Rect2(origin,CELL*Vector2.ONE),color)
		_draw_terrain_detail(terrain_type,origin,seed)
	for resource: Dictionary in resources:
		var resource_type := int(resource.type)
		var color := Color("#a9b4bd") if resource_type == 1 else (Color("#24272c") if resource_type == 3 else Color("#c2763b"))
		var remaining := int(resource.remaining)
		if bool(resource.get("depleted", false)): color = Color("#39414a")
		var center := Vector2(
			(float(resource.x) + 0.5) * CELL,
			(float(resource.y) + 0.5) * CELL
		)
		var seed := decoration_signature(Vector2i(int(resource.x),int(resource.y)))
		var richness := clampi(remaining / 100 + 3,3,10) if remaining > 0 else 2
		draw_circle(center,31.0,color.darkened(0.68 if remaining > 0 else 0.35))
		for fragment in range(richness):
			var angle := float((seed + fragment * 97) % 628) / 100.0
			var distance := 7.0 + float((seed / (fragment + 1) + fragment * 31) % 20)
			var p := center + Vector2(cos(angle),sin(angle)) * distance
			var size := 3.5 + float((seed + fragment * 17) % 5)
			draw_colored_polygon(PackedVector2Array([
				p+Vector2(-size,1),p+Vector2(-1,-size),
				p+Vector2(size,-1),p+Vector2(1,size)]),color.lightened(float(fragment%3)*0.08))
		if remaining == 0:
			draw_line(center + Vector2(-10,-10),center + Vector2(10,10),Color("#9ba4ad"),3.0)
			draw_line(center + Vector2(10,-10),center + Vector2(-10,10),Color("#9ba4ad"),3.0)
		if remaining > 0:
			draw_string(ThemeDB.fallback_font,center+Vector2(-28,31),
				"%d" % remaining,HORIZONTAL_ALIGNMENT_CENTER,56,10,Color("#edf2f4",0.82))
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
		elif block_id != 0 and selected_reserved_blocks.has(block_id):
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

func _draw_terrain_detail(terrain_type: int,origin: Vector2,seed: int) -> void:
	if terrain_type == 2:
		var phase := cosmetic_time * 0.75 + float(seed % 31) * 0.12
		for row in range(2):
			var y := 23.0 + float(row) * 27.0 + sin(phase + row) * 2.0
			draw_arc(origin+Vector2(38,y),18.0,0.15,PI-0.15,12,Color("#5da7ba",0.27),1.5)
	elif terrain_type == 3:
		for mark in range(4):
			var x := 10.0 + float((seed + mark * 23) % 55)
			var y := 12.0 + float((seed / (mark + 1) + mark * 19) % 50)
			var r := 3.0 + float((seed + mark) % 5)
			draw_colored_polygon(PackedVector2Array([
				origin+Vector2(x-r,y+2),origin+Vector2(x-1,y-r),
				origin+Vector2(x+r,y),origin+Vector2(x+1,y+r)]),Color("#64686b",0.55))
	else:
		for mark in range(2):
			var x := 8.0 + float((seed + mark * 37) % 60)
			var y := 9.0 + float((seed / (mark + 1) + mark * 29) % 58)
			var tint := Color("#6e8057",0.26) if (seed + mark) % 3 else Color("#8b7652",0.22)
			draw_line(origin+Vector2(x,y),origin+Vector2(x+3,y-5),tint,1.2)
