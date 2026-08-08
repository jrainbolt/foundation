class_name FoundationEntityVisual
extends Node2D

const CELL := 76.0
const TILE_RECT := Rect2(4, 4, 68, 68)
const COLORS := {
	1: Color("#376e4d"), 2: Color("#896a25"), 3: Color("#7e403d"),
	4: Color("#765028"), 5: Color("#365d87"), 6: Color("#98612f"),
	7: Color("#744a68"), 8: Color("#55616b"), 9: Color("#857322"),
	10: Color("#286d80"), 11: Color("#28778d"), 12: Color("#286d80"),
	13: Color("#8b4d2d"), 14: Color("#857322"), 15: Color("#857322"),
	16: Color("#64752b"), 17: Color("#39754f"), 18: Color("#873d8c"),
	19: Color("#873d8c"), 20: Color("#857322"), 21: Color("#286d80"),
}
const TITLES := {
	1: "EXTRACTOR", 2: "BELT", 3: "REFINERY", 4: "ASSEMBLER",
	5: "STORAGE", 6: "SPLITTER", 7: "INSERTER", 8: "POWER POLE",
	9: "GENERATOR", 10: "FLUID TANK", 11: "PIPE", 12: "WATER SOURCE",
	13: "BOILER", 14: "STEAM ENGINE", 15: "SOLAR", 16: "ACCUMULATOR",
	17: "REACTOR", 18: "HEAT PIPE", 19: "HEAT EXCHANGER",
	20: "TURBINE", 21: "CONDENSER",
}
const ABBREVIATIONS := {
	1: "EX", 2: "BELT", 3: "REF", 4: "ASM", 5: "BOX", 6: "SPLIT",
	7: "INS", 8: "POLE", 9: "GEN", 10: "TANK", 11: "PIPE",
	12: "WATER", 13: "BOIL", 14: "STEAM", 15: "SOLAR", 16: "ACC",
	17: "CORE", 18: "HEAT", 19: "HEX", 20: "TURB", 21: "COND",
}

var state: Dictionary = {}
var selected := false
var hovered := false
var title_font := SystemFont.new()

func _init() -> void:
	title_font.font_names = PackedStringArray(["Helvetica", "Arial", "Sans"])
	title_font.font_weight = 700

func set_selected(value: bool) -> void:
	selected = value
	queue_redraw()

func set_hovered(value: bool) -> void:
	hovered = value
	queue_redraw()

func apply(next_state: Dictionary) -> void:
	state = next_state.duplicate(true)
	position = Vector2(float(state.x) * CELL, float(state.y) * CELL)
	queue_redraw()

func _draw() -> void:
	var entity_type := int(state.get("type", 0))
	var color: Color = COLORS.get(entity_type, Color("#505862"))
	if not bool(state.get("powered", true)) and entity_type in [1, 3, 4, 7, 21]:
		color = color.darkened(0.42)
	draw_rect(TILE_RECT, Color("#111820"), true)
	draw_rect(Rect2(6, 6, 64, 64), color, true)
	draw_rect(TILE_RECT, color.lightened(0.28), false, 1.5)
	_draw_connections(entity_type)
	draw_string(title_font, Vector2(7, 18), TITLES.get(entity_type, "ENTITY"), HORIZONTAL_ALIGNMENT_CENTER, 62, 10, Color("#e8edf2"))
	_draw_centered(ABBREVIATIONS.get(entity_type, "?"), 44.0, 18, Color.WHITE)
	var status := _important_status(entity_type)
	if not status.is_empty():
		_draw_centered(status, 64.0, 10, Color("#d6e0e8"))
	_draw_direction()
	_draw_resource_badge()
	_draw_process_bar()
	if hovered:
		draw_rect(Rect2(2, 2, 72, 72), Color("#f5fbff", 0.92), false, 1.5)
	if selected:
		draw_rect(Rect2(0, 0, 76, 76), Color("#fff176", 0.18), false, 7.0)
		draw_rect(Rect2(1.5, 1.5, 73, 73), Color("#fff176"), false, 3.5)
		draw_circle(Vector2(66, 10), 7.0, Color("#fff176"))
		draw_string(ThemeDB.fallback_font, Vector2(63, 14), "S", HORIZONTAL_ALIGNMENT_CENTER, 7, 9, Color("#20252d"))

func _draw_centered(text: String, baseline: float, size: int, color: Color) -> void:
	draw_string(ThemeDB.fallback_font, Vector2(7, baseline), text, HORIZONTAL_ALIGNMENT_CENTER, 62, size, color)

func _important_status(entity_type: int) -> String:
	match entity_type:
		1, 3, 4:
			var duration := int(state.get("duration", 0))
			return "%d%%" % (int(state.get("progress", 0)) * 100 / duration) if duration > 0 else "IDLE"
		5:
			var used := int(state.get("output_quantity", 0))
			for quantity: int in state.get("inventory", []): used += quantity
			return "%d STORED" % used
		7:
			return "%d ITEMS" % int(state.get("quantity", 0))
		8:
			return "NET %d" % int(state.get("network", state.get("power_network_id", 0)))
		9, 14, 15, 20:
			return "%d OUTPUT" % int(state.get("generated_last_tick", 0))
		10:
			return "%d / %d" % [int(state.get("fluid_quantity", 0)), int(state.get("fluid_capacity", 0))]
		12:
			return "%d WATER" % int(state.get("stored_water", 0))
		13, 19:
			return "%d STEAM" % int(state.get("stored_steam", 0))
		16:
			return "%d / %d" % [int(state.get("stored_energy", 0)), int(state.get("capacity", 0))]
		17:
			return "%d HEAT" % int(state.get("stored_heat", 0))
		18:
			return "NET %d" % int(state.get("heat_network_id", 0))
		21:
			return "%d WATER" % int(state.get("stored_water", 0))
	return ""

func _draw_connections(entity_type: int) -> void:
	if entity_type not in [11, 18]: return
	var mask := int(state.get("connection_mask", 0))
	var center := Vector2(38, 38)
	var vectors := [Vector2.UP, Vector2.RIGHT, Vector2.DOWN, Vector2.LEFT]
	var color := Color("#63d9ee", 0.82) if entity_type == 11 else Color("#ed72ef", 0.82)
	for index in 4:
		if mask & (1 << index): draw_line(center, center + vectors[index] * 32.0, color, 8.0)

func _draw_direction() -> void:
	var direction := int(state.get("direction", -1))
	if direction < 0: return
	var vectors := [Vector2.UP, Vector2.RIGHT, Vector2.DOWN, Vector2.LEFT]
	var start := Vector2(59, 56)
	draw_line(start, start + vectors[direction] * 9.0, Color.WHITE, 2.5)

func _draw_resource_badge() -> void:
	var resource_type := int(state.get("resource_type", 0))
	if resource_type == 0: return
	var color := Color("#a85f42") if resource_type == 1 else Color("#c58145")
	draw_circle(Vector2(10, 10), 6.0, color)
	draw_circle(Vector2(10, 10), 6.0, Color.WHITE, false, 1.0)

func _draw_process_bar() -> void:
	var duration := int(state.get("duration", 0))
	if duration <= 0: return
	var ratio := clampf(float(state.get("progress", 0)) / float(duration), 0.0, 1.0)
	draw_rect(Rect2(9, 67, 58, 3), Color("#18202a"), true)
	draw_rect(Rect2(9, 67, 58.0 * ratio, 3), Color("#f6d36c"), true)
