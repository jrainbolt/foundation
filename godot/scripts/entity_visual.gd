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
	29: Color("#4a5963"), 30: Color("#52606d"),
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
	29: "RAIL SIGNAL", 30: "CHAIN SIGNAL",
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
var cosmetic_time := 0.0
var movement_from := Vector2.ZERO
var movement_target := Vector2.ZERO
var movement_elapsed := 1.0
var visual_step_elapsed := 0.0
const VEHICLE_LERP_SECONDS := 0.30

func _init() -> void:
	title_font.font_names = PackedStringArray(["Helvetica", "Arial", "Sans"])
	title_font.font_weight = 700
	set_process(true)

func _process(delta: float) -> void:
	cosmetic_time = fmod(cosmetic_time + delta,1000.0)
	visual_step_elapsed = minf(1.0,visual_step_elapsed + delta * 12.0)
	if movement_elapsed < VEHICLE_LERP_SECONDS:
		movement_elapsed = minf(VEHICLE_LERP_SECONDS,movement_elapsed + delta)
		var ratio := smoothstep(0.0,1.0,movement_elapsed / VEHICLE_LERP_SECONDS)
		position = movement_from.lerp(movement_target,ratio)
	queue_redraw()

func set_selected(value: bool) -> void:
	selected = value
	queue_redraw()

func set_hovered(value: bool) -> void:
	hovered = value
	queue_redraw()

func apply(next_state: Dictionary) -> void:
	var entity_type := int(next_state.get("type",0))
	var next_position := Vector2(float(next_state.x) * CELL,float(next_state.y) * CELL)
	if not state.is_empty() and entity_type in [27,28] and next_position != movement_target:
		movement_from = position
		movement_target = next_position
		movement_elapsed = 0.0
	else:
		movement_from = next_position
		movement_target = next_position
		position = next_position
		movement_elapsed = VEHICLE_LERP_SECONDS
	state = next_state.duplicate(true)
	visual_step_elapsed = 0.0
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
	if entity_type == 30:
		_draw_signal()
		draw_circle(Vector2(38,42),5.0,Color("#dce8f2"))
		_draw_selection()
		return
	if entity_type == 2:
		_draw_belt(color)
		_draw_selection()
		return
	if entity_type == 7:
		_draw_inserter(color)
		_draw_selection()
		return
	if not bool(state.get("powered", true)) and entity_type in [1, 3, 4, 7, 21, 22]:
		color = color.darkened(0.42)
	draw_rect(TILE_RECT, Color("#111820"), true)
	draw_rect(Rect2(6, 6, 64, 64), color, true)
	draw_rect(TILE_RECT, color.lightened(0.28), false, 1.5)
	_draw_connections(entity_type)
	draw_string(title_font, Vector2(7, 18), TITLES.get(entity_type, "ENTITY"), HORIZONTAL_ALIGNMENT_CENTER, 62, 10, Color("#e8edf2"))
	_draw_machine_silhouette(entity_type,color)
	var status := _important_status(entity_type)
	if not status.is_empty():
		_draw_centered(status, 64.0, 10, Color("#d6e0e8"))
	_draw_direction()
	_draw_resource_badge()
	_draw_process_bar()
	_draw_selection()

func _draw_belt(color: Color) -> void:
	var direction := int(state.get("direction",1))
	var vectors := [Vector2.UP,Vector2.RIGHT,Vector2.DOWN,Vector2.LEFT]
	var axis: Vector2 = vectors[direction]
	var side := axis.rotated(PI/2.0)
	var center := Vector2(38,38)
	draw_rect(TILE_RECT,Color("#10161b"),true)
	draw_line(center-axis*38.0,center+axis*38.0,Color("#242b31"),32.0)
	for offset in [-12.0,12.0]:
		draw_line(center-axis*38.0+side*offset,center+axis*38.0+side*offset,color.lightened(0.25),3.0)
	for tread in range(-2,3):
		var p := center + axis*(float(tread)*15.0-4.0)
		draw_line(p-side*11.0,p+side*11.0,Color("#9e7f37",0.75),2.0)
	var item := int(state.get("item",0))
	if item != 0:
		var duration := maxi(1,int(state.get("duration",1)))
		var ratio := clampf((float(state.get("progress",0))+visual_step_elapsed)/float(duration),0.0,1.0)
		var token := center + axis*((ratio-0.5)*48.0)
		_draw_item_token(token,item,6.0)
	draw_line(center-axis*7.0,center+axis*10.0,Color("#f1ce65"),3.0)
	draw_colored_polygon(PackedVector2Array([center+axis*15.0,center+axis*7.0+side*5.0,center+axis*7.0-side*5.0]),Color("#f1ce65"))

func _draw_inserter(color: Color) -> void:
	draw_rect(TILE_RECT,Color("#10161b"),true)
	draw_circle(Vector2(38,38),24.0,color.darkened(0.18))
	draw_circle(Vector2(38,38),9.0,Color("#c8b7d1"))
	var direction := int(state.get("direction",1))
	var axis: Vector2 = [Vector2.UP,Vector2.RIGHT,Vector2.DOWN,Vector2.LEFT][direction]
	var inserter_state := int(state.get("inserter_state",0))
	var progress := clampf(maxf(float(state.get("progress",0)),visual_step_elapsed),0.0,1.0)
	var phase := 0.0
	if inserter_state == 1: phase = -1.0 + progress
	elif inserter_state == 2: phase = 0.0
	elif inserter_state == 3: phase = progress
	else: phase = -1.0
	var head := Vector2(38,38) + axis * phase * 24.0
	draw_line(Vector2(38,38),head,Color("#e2c457"),7.0)
	draw_circle(head,7.0,Color("#f5da72"))
	if int(state.get("item",0)) != 0:
		_draw_item_token(head,int(state.item),4.5)
	_draw_centered("WAIT" if inserter_state == 0 else "MOVE",68.0,9,Color("#d8e0e7"))

func _draw_machine_silhouette(entity_type: int,color: Color) -> void:
	var center := Vector2(38,39)
	var active := _is_visually_active(entity_type)
	var pulse := 0.5 + 0.5*sin(cosmetic_time*7.0)
	match entity_type:
		1:
			draw_circle(center,17.0,Color("#182029"))
			for arm in range(4):
				var angle := float(arm)*PI/2.0+(cosmetic_time*2.2 if active else 0.0)
				draw_line(center+Vector2(cos(angle),sin(angle))*6.0,center+Vector2(cos(angle),sin(angle))*20.0,color.lightened(0.38),5.0)
			draw_circle(center,6.0,Color("#d9e2e7"))
		3:
			draw_rect(Rect2(20,25,36,30),Color("#1a2026"),true)
			draw_circle(center,13.0,Color("#ff9b4a",0.35+0.45*pulse if active else 0.18))
			draw_line(Vector2(24,24),Vector2(24,16),Color("#aeb7bd"),5.0)
		4:
			draw_circle(center,18.0,Color("#19222a"))
			var angle := cosmetic_time*2.8 if active else 0.0
			for tooth in range(6):
				var a := angle+float(tooth)*TAU/6.0
				draw_circle(center+Vector2(cos(a),sin(a))*14.0,4.0,color.lightened(0.4))
			draw_circle(center,7.0,Color("#d6e0e5"))
		5:
			draw_rect(Rect2(18,24,40,33),Color("#1b2732"),true)
			draw_line(Vector2(18,34),Vector2(58,34),color.lightened(0.35),4.0)
			draw_line(Vector2(38,24),Vector2(38,57),color.lightened(0.25),3.0)
		9,14,20:
			draw_circle(center,19.0,Color("#192127"))
			draw_arc(center,14.0,0.0,TAU,24,color.lightened(0.4),5.0)
			var needle := cosmetic_time*3.0 if active else -PI/2.0
			draw_line(center,center+Vector2(cos(needle),sin(needle))*13.0,Color("#f8df72"),4.0)
		10:
			draw_rect(Rect2(21,20,34,39),Color("#16252c"),true)
			draw_arc(Vector2(38,21),17.0,PI,TAU,18,color.lightened(0.42),3.0)
			draw_rect(Rect2(24,43,28,12),Color("#4fd5ee",0.55),true)
		12:
			draw_circle(center,18.0,Color("#14303a"))
			draw_colored_polygon(PackedVector2Array([Vector2(38,20),Vector2(55,49),Vector2(21,49)]),Color("#5bd6ec",0.62))
		13,19,21:
			draw_rect(Rect2(19,24,38,31),Color("#192229"),true)
			draw_circle(center,12.0,color.lightened(0.22))
			if active:
				for bubble in range(3): draw_circle(Vector2(27+bubble*11,23-fmod(cosmetic_time*15.0+bubble*7.0,11.0)),2.5,Color("#d9f7ff",0.75))
		16:
			draw_rect(Rect2(21,20,34,39),Color("#172027"),true)
			draw_colored_polygon(PackedVector2Array([Vector2(40,23),Vector2(29,41),Vector2(38,41),Vector2(34,55),Vector2(49,35),Vector2(40,35)]),Color("#f6dc62"))
		17:
			draw_circle(center,19.0,Color("#17271f"))
			draw_circle(center,12.0,Color("#72e59b",0.30+0.5*pulse if active else 0.24))
		22:
			draw_rect(Rect2(20,22,36,36),Color("#172333"),true)
			draw_circle(center,14.0,Color("#75b9ff",0.30+0.45*pulse if active else 0.22))
			draw_line(Vector2(28,51),Vector2(48,27),Color("#dff3ff"),3.0)
			draw_line(Vector2(28,27),Vector2(48,51),Color("#dff3ff"),3.0)
		23:
			draw_rect(Rect2(17,27,42,29),Color("#202a23"),true)
			for crate in range(3): draw_rect(Rect2(20+crate*12,31,10,18),color.lightened(0.2+crate*0.06),true)
		_:
			_draw_centered(ABBREVIATIONS.get(entity_type,"?"),46.0,16,Color.WHITE)

func _is_visually_active(entity_type: int) -> bool:
	if not bool(state.get("powered",true)): return false
	if entity_type in [1,3,4]: return int(state.get("duration",0)) > 0 and int(state.get("progress",0)) > 0
	if entity_type == 22: return int(state.get("lab_activity",0)) == 1
	if entity_type in [9,14,15,20]: return int(state.get("generated_last_tick",0)) > 0
	if entity_type == 13: return bool(state.get("conversion_active",false))
	if entity_type == 19: return int(state.get("heat_exchanger_activity",0)) == 1
	if entity_type == 21: return int(state.get("condenser_activity",0)) == 1
	return false

func _draw_item_token(at: Vector2,item: int,radius: float) -> void:
	var colors := [Color("#dce5ea"),Color("#aeb8bf"),Color("#c77b42"),Color("#e68a48"),Color("#72cce3"),Color("#a7d9e6"),Color("#dfb94e"),Color("#88c56c"),Color("#c86654"),Color("#835f43"),Color("#31353a"),Color("#8ea4b4"),Color("#9d72c7"),Color("#7cceeb")]
	var token_color: Color = colors[clampi(item-1,0,colors.size()-1)]
	draw_circle(at,radius+2.0,Color("#111820"))
	draw_circle(at,radius,token_color)

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
	var mode := int(state.get("freight_mode",0))
	var capacity := maxi(1,int(state.get("freight_capacity",200)))
	var fill := clampf(float(state.get("freight_quantity",0))/float(capacity),0.0,1.0)
	var freight_color := Color("#e4b84b") if mode == 1 else Color("#4bc1d9")
	draw_rect(Rect2(15,43,46,8),Color("#172027"),true)
	draw_rect(Rect2(15,43,46*fill,8),freight_color,true)
	draw_line(Vector2(12,55),Vector2(64,55),Color("#c8d1d6"),5.0)
	_draw_centered(["OFF","LOAD","UNLOAD"][clampi(mode,0,2)],37.0,11,Color.WHITE)
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
	var color := Color("#a85f42") if resource_type == 1 else (Color("#30343b") if resource_type == 3 else Color("#c58145"))
	if int(state.get("resource_remaining", 1)) == 0: color = Color("#56606a")
	draw_circle(Vector2(10, 10), 6.0, color)
	draw_circle(Vector2(10, 10), 6.0, Color.WHITE, false, 1.0)

func _draw_process_bar() -> void:
	var duration := int(state.get("duration", 0))
	if duration <= 0: return
	var ratio := clampf(float(state.get("progress", 0)) / float(duration), 0.0, 1.0)
	draw_rect(Rect2(9, 67, 58, 3), Color("#18202a"), true)
	draw_rect(Rect2(9, 67, 58.0 * ratio, 3), Color("#f6d36c"), true)
