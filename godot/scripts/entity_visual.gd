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
	22: Color("#475d91"),
	23: Color("#596b46"),
	24: Color("#9ea7af"), 25: Color("#506a78"), 26: Color("#a88946"), 27: Color("#b64b3b"), 28: Color("#3f7294"),
	29: Color("#4a5963"),
}
const TITLES := {
	1: "EXTRACTOR", 2: "BELT", 3: "REFINERY", 4: "ASSEMBLER",
	5: "STORAGE", 6: "SPLITTER", 7: "INSERTER", 8: "POWER POLE",
	9: "GENERATOR", 10: "FLUID TANK", 11: "PIPE", 12: "WATER SOURCE",
	13: "BOILER", 14: "STEAM ENGINE", 15: "SOLAR", 16: "ACCUMULATOR",
	17: "REACTOR", 18: "HEAT PIPE", 19: "HEAT EXCHANGER",
	20: "TURBINE", 21: "CONDENSER",
	22: "RESEARCH LAB",
	23: "CONSTRUCTION DEPOT",
	24: "RAIL", 25: "RAIL STATION", 26: "RAIL SWITCH", 27: "LOCOMOTIVE", 28: "CARGO WAGON",
	29: "RAIL SIGNAL",
}
const ABBREVIATIONS := {
	1: "EX", 2: "BELT", 3: "REF", 4: "ASM", 5: "BOX", 6: "SPLIT",
	7: "INS", 8: "POLE", 9: "GEN", 10: "TANK", 11: "PIPE",
	12: "WATER", 13: "BOIL", 14: "STEAM", 15: "SOLAR", 16: "ACC",
	17: "CORE", 18: "HEAT", 19: "HEX", 20: "TURB", 21: "COND",
	22: "LAB",
	23: "DEPOT",
	24: "", 25: "STN", 26: "SW", 27: "LOCO", 28: "CARGO",
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
	if entity_type == 24:
		_draw_rail()
		_draw_selection()
		return
	if entity_type == 25:
		_draw_station(color)
		_draw_selection()
		return
	if entity_type == 26:
		_draw_switch()
		_draw_selection()
		return
	if entity_type == 27:
		_draw_locomotive(color)
		_draw_selection()
		return
	if entity_type == 28:
		_draw_cargo_wagon(color)
		_draw_selection()
		return
	if entity_type == 29:
		_draw_signal()
		_draw_selection()
		return
	if not bool(state.get("powered", true)) and entity_type in [1, 3, 4, 7, 21, 22]:
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
	_draw_selection()

func _draw_signal() -> void:
	var aspect := int(state.get("signal_aspect",1))
	var lamp := Color("#4ee579") if aspect == 0 else (Color("#55d7ff") if aspect == 2 else Color("#f05b55"))
	draw_line(Vector2(38,62),Vector2(38,24),Color("#b6c1ca"),5.0)
	draw_rect(Rect2(27,12,22,22),Color("#172029"),true)
	draw_circle(Vector2(38,23),7.0,lamp)
	var direction := int(state.get("signal_orientation",state.get("direction",0)))
	var vectors := [Vector2(0,-1),Vector2(1,0),Vector2(0,1),Vector2(-1,0)]
	var arrow: Vector2 = vectors[direction]
	draw_line(Vector2(38,49),Vector2(38,49)+arrow*13.0,lamp,3.0)

func _draw_selection() -> void:
	if hovered:
		draw_rect(Rect2(2, 2, 72, 72), Color("#f5fbff", 0.92), false, 1.5)
	if selected:
		draw_rect(Rect2(0, 0, 76, 76), Color("#fff176", 0.18), false, 7.0)
		draw_rect(Rect2(1.5, 1.5, 73, 73), Color("#fff176"), false, 3.5)
		draw_circle(Vector2(66, 10), 7.0, Color("#fff176"))
		draw_string(ThemeDB.fallback_font, Vector2(63, 14), "S", HORIZONTAL_ALIGNMENT_CENTER, 7, 9, Color("#20252d"))

func _draw_rail() -> void:
	var center := Vector2(38, 38)
	var vectors := [Vector2.UP, Vector2.RIGHT, Vector2.DOWN, Vector2.LEFT]
	var mask := int(state.get("port_mask", 0))
	for index in 4:
		if mask & (1 << index):
			draw_line(center, center + vectors[index] * 38.0, Color("#252b30"), 13.0)
			draw_line(center, center + vectors[index] * 38.0, Color("#aeb8bf"), 7.0)
	draw_circle(center, 6.0, Color("#d4dde3"))
	draw_string(ThemeDB.fallback_font,Vector2(5,14),"N%d" % int(state.get("rail_network_id",0)),HORIZONTAL_ALIGNMENT_LEFT,-1,9,Color("#dce7ee"))

func _draw_locomotive(color: Color) -> void:
	var center := Vector2(38,38)
	var direction := int(state.get("travel_direction",state.get("direction",1)))
	var angle: float = [ -PI/2.0, 0.0, PI/2.0, PI ][direction]
	draw_set_transform(center,angle,Vector2.ONE)
	draw_rect(Rect2(-27,-18,48,36),Color("#14191e"),true)
	draw_rect(Rect2(-24,-15,42,30),color,true)
	draw_colored_polygon(PackedVector2Array([Vector2(18,-15),Vector2(29,0),Vector2(18,15)]),color.lightened(0.25))
	draw_circle(Vector2(-14,-19),5,Color("#242a2f"));draw_circle(Vector2(13,-19),5,Color("#242a2f"))
	draw_circle(Vector2(-14,19),5,Color("#242a2f"));draw_circle(Vector2(13,19),5,Color("#242a2f"))
	draw_set_transform(Vector2.ZERO,0.0,Vector2.ONE)
	draw_string(title_font,Vector2(8,12),"LOCO",HORIZONTAL_ALIGNMENT_CENTER,60,10,Color.WHITE)

func _draw_cargo_wagon(color: Color) -> void:
	var center := Vector2(38,38)
	var direction := int(state.get("entry_direction",state.get("direction",3)))
	var angle: float = [ -PI/2.0, 0.0, PI/2.0, PI ][direction]
	draw_set_transform(center,angle,Vector2.ONE)
	draw_rect(Rect2(-27,-18,54,36),Color("#14191e"),true)
	draw_rect(Rect2(-23,-14,46,28),color,true)
	var capacity: int = maxi(1,int(state.get("cargo_capacity",100)))
	var fill: float = float(state.get("cargo_quantity",0))/float(capacity)
	draw_rect(Rect2(-20,8,40*fill,4),color.lightened(0.35),true)
	draw_circle(Vector2(-15,-19),5,Color("#242a2f"));draw_circle(Vector2(15,-19),5,Color("#242a2f"))
	draw_circle(Vector2(-15,19),5,Color("#242a2f"));draw_circle(Vector2(15,19),5,Color("#242a2f"))
	draw_set_transform(Vector2.ZERO,0.0,Vector2.ONE)
	draw_string(title_font,Vector2(8,12),"CARGO",HORIZONTAL_ALIGNMENT_CENTER,60,10,Color.WHITE)

func _draw_station(color: Color) -> void:
	draw_rect(Rect2(8,14,60,48),Color("#111820"),true)
	draw_rect(Rect2(11,17,54,42),color,true)
	draw_line(Vector2(12,55),Vector2(64,55),Color("#c8d1d6"),5.0)
	_draw_centered("STN",43.0,18,Color.WHITE)
	_draw_centered("CONNECTED" if bool(state.get("rail_connected",false)) else "NO RAIL",57.0,9,Color("#cfe7d4") if bool(state.get("rail_connected",false)) else Color("#ff9f91"))

func _draw_switch() -> void:
	var center := Vector2(38, 38)
	var vectors := [Vector2.UP, Vector2.RIGHT, Vector2.DOWN, Vector2.LEFT]
	var mask := int(state.get("port_mask", 0))
	var stem := int(state.get("stem_direction", 0))
	var selected := int(state.get("branch_a_direction", 1)) if int(state.get("selected_branch", 0)) == 0 else int(state.get("branch_b_direction", 3))
	for index in range(4):
		if mask & (1 << index):
			draw_line(center, center + vectors[index] * 38.0, Color("#343b40"), 13.0)
			draw_line(center, center + vectors[index] * 38.0, Color("#69747b"), 5.0)
	for index in [stem, selected]:
		draw_line(center, center + vectors[index] * 38.0, Color("#f4d06f"), 8.0)
	draw_circle(center, 7.0, Color("#fff0b0"))
	draw_string(ThemeDB.fallback_font,Vector2(5,14),"SW %s" % ("A" if int(state.get("selected_branch",0)) == 0 else "B"),HORIZONTAL_ALIGNMENT_LEFT,-1,9,Color.WHITE)

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
		22:
			return "%d / %d SCI" % [int(state.get("science_quantity", 0)), int(state.get("science_capacity", 0))]
		23:
			return "%d / %d MAT" % [int(state.get("material_quantity", 0)), int(state.get("capacity", 0))]
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
	if int(state.get("resource_remaining", 1)) == 0: color = Color("#56606a")
	draw_circle(Vector2(10, 10), 6.0, color)
	draw_circle(Vector2(10, 10), 6.0, Color.WHITE, false, 1.0)

func _draw_process_bar() -> void:
	var duration := int(state.get("duration", 0))
	if duration <= 0: return
	var ratio := clampf(float(state.get("progress", 0)) / float(duration), 0.0, 1.0)
	draw_rect(Rect2(9, 67, 58, 3), Color("#18202a"), true)
	draw_rect(Rect2(9, 67, 58.0 * ratio, 3), Color("#f6d36c"), true)
