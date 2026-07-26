class_name FoundationEntityVisual
extends Node2D

const COLORS := {
	1: Color("#57a773"), 2: Color("#d6a84b"), 3: Color("#d46a6a"),
	4: Color("#8974c9"), 5: Color("#5f8fca"), 6: Color("#e1903f"),
	7: Color("#cc79a7"), 8: Color("#8a9ba8"), 9: Color("#e5c84d"),
	10: Color("#4da6c8"),
	11: Color("#7fc8d8"),
	12: Color("#3388bb"),
	13: Color("#b86b3c"),
	14: Color("#7aa6d8"),
	15: Color("#f0b429"),
	16: Color("#8bd450"),
	17: Color("#6ee7b7"),
}
const NAMES := {
	1: "EX", 2: "BELT", 3: "REF", 4: "ASM", 5: "BOX",
	6: "SPLIT", 7: "INS", 8: "POLE", 9: "GEN", 10: "TANK",
	11: "PIPE",
	12: "WATER", 13: "BOIL",
	14: "STEAM",
	15: "SOLAR",
	16: "ACC",
	17: "CORE",
}

var state: Dictionary = {}

func apply(next_state: Dictionary) -> void:
	state = next_state.duplicate(true)
	position = Vector2(float(state.x) * 64.0, float(state.y) * 64.0)
	queue_redraw()

func _draw() -> void:
	var entity_type: int = int(state.get("type", 0))
	var color: Color = COLORS.get(entity_type, Color.GRAY)
	if not bool(state.get("powered", true)) and entity_type in [1, 3, 4, 7]:
		color = color.darkened(0.55)
	draw_rect(Rect2(4, 4, 56, 56), color, true)
	draw_rect(Rect2(4, 4, 56, 56), Color("#1d2430"), false, 2.0)
	draw_string(
		ThemeDB.fallback_font, Vector2(7, 24),
		NAMES.get(entity_type, "?"), HORIZONTAL_ALIGNMENT_LEFT, -1, 12
	)
	draw_string(
		ThemeDB.fallback_font, Vector2(7, 53),
		"#%d" % int(state.get("id", 0)), HORIZONTAL_ALIGNMENT_LEFT, -1, 11
	)
	if entity_type == 9:
		draw_string(
			ThemeDB.fallback_font, Vector2(7, 39),
			"F%d E%d %s" % [
				int(state.get("fuel_ticks", 0)),
				int(state.get("energy_available", 0)),
				"ON" if bool(state.get("fuel_active", false)) else "OFF"
			],
			HORIZONTAL_ALIGNMENT_LEFT, -1, 9
		)
	if entity_type == 10:
		draw_string(
			ThemeDB.fallback_font, Vector2(7, 39),
			"F%d %d/%d" % [
				int(state.get("fluid_type", 0)),
				int(state.get("fluid_quantity", 0)),
				int(state.get("fluid_capacity", 0))
			],
			HORIZONTAL_ALIGNMENT_LEFT, -1, 9
		)
	if entity_type == 11:
		var mask := int(state.get("connection_mask", 0))
		var center := Vector2(32, 32)
		var vectors := [Vector2.UP, Vector2.RIGHT, Vector2.DOWN, Vector2.LEFT]
		for index in 4:
			if mask & (1 << index):
				draw_line(center, center + vectors[index] * 28.0, Color.WHITE, 8.0)
		draw_string(
			ThemeDB.fallback_font, Vector2(7, 39),
			"N%d" % int(state.get("network_id", 0)),
			HORIZONTAL_ALIGNMENT_LEFT, -1, 9
		)
	if entity_type == 12:
		draw_string(
			ThemeDB.fallback_font, Vector2(7, 39),
			"W %d/%d" % [
				int(state.get("stored_water", 0)),
				int(state.get("output_capacity", 0))
			],
			HORIZONTAL_ALIGNMENT_LEFT, -1, 9
		)
	if entity_type == 13:
		draw_string(
			ThemeDB.fallback_font, Vector2(7, 39),
			"W%d S%d %s" % [
				int(state.get("stored_water", 0)),
				int(state.get("stored_steam", 0)),
				"ON" if bool(state.get("conversion_active", false)) else "OFF"
			],
			HORIZONTAL_ALIGNMENT_LEFT, -1, 9
		)
	if entity_type == 14:
		draw_string(
			ThemeDB.fallback_font, Vector2(7, 39),
			"S%d G%d %s" % [
				int(state.get("stored_steam", 0)),
				int(state.get("generated_last_tick", 0)),
				"ON" if bool(state.get("generation_active", false)) else "OFF"
			],
			HORIZONTAL_ALIGNMENT_LEFT, -1, 9
		)
	if entity_type == 15:
		draw_string(
			ThemeDB.fallback_font, Vector2(7, 39),
			"G%d/%d %s" % [
				int(state.get("generated_last_tick", 0)),
				int(state.get("available_generation", 0)),
				"ON" if bool(state.get("generation_active", false)) else "OFF"
			],
			HORIZONTAL_ALIGNMENT_LEFT, -1, 9
		)
	if entity_type == 16:
		draw_string(
			ThemeDB.fallback_font, Vector2(7, 39),
			"%d/%d C%d D%d" % [
				int(state.get("stored_energy", 0)),
				int(state.get("capacity", 0)),
				int(state.get("charged_last_tick", 0)),
				int(state.get("discharged_last_tick", 0))
			],
			HORIZONTAL_ALIGNMENT_LEFT, -1, 8
		)
	if entity_type == 17:
		draw_string(
			ThemeDB.fallback_font, Vector2(7, 39),
			"H%d/%d F%d %s" % [
				int(state.get("stored_heat", 0)),
				int(state.get("heat_capacity", 0)),
				int(state.get("remaining_burn_ticks", 0)),
				"ON" if int(state.get("reactor_activity", 0)) == 1 else "OFF"
			],
			HORIZONTAL_ALIGNMENT_LEFT, -1, 8
		)
	var duration := int(state.get("duration", 0))
	if duration > 0:
		var ratio: float = clampf(
			float(state.get("progress", 0)) / float(duration), 0.0, 1.0
		)
		draw_rect(Rect2(6, 57, 52, 4), Color("#242b35"), true)
		draw_rect(Rect2(6, 57, 52.0 * ratio, 4), Color.WHITE, true)
	var direction := int(state.get("direction", -1))
	if direction >= 0:
		var vectors := [Vector2.UP, Vector2.RIGHT, Vector2.DOWN, Vector2.LEFT]
		var center := Vector2(32, 32)
		draw_line(center, center + vectors[direction] * 18.0, Color.WHITE, 3.0)
	var resource_type := int(state.get("resource_type", 0))
	if resource_type != 0:
		var resource_color := (
			Color("#984f35") if resource_type == 1 else Color("#b87333")
		)
		draw_circle(Vector2(9, 9), 7.0, resource_color)
		draw_circle(Vector2(9, 9), 7.0, Color.WHITE, false, 1.0)
