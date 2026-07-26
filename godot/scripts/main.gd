extends Control

const MAX_EVENT_LINES := 80
const STEPS_PER_SECOND := 12.0

@onready var canvas: FoundationFactoryCanvas = %FactoryCanvas
@onready var tick_label: Label = %TickLabel
@onready var status_label: Label = %StatusLabel
@onready var event_log: RichTextLabel = %EventLog
@onready var run_button: Button = %RunButton

var simulation: Object
var running := false
var cadence_accumulator := 0.0
var event_lines: Array[String] = []

func _ready() -> void:
	if not ClassDB.class_exists("FoundationSimulation"):
		status_label.text = "Status: native extension failed to load"
		set_physics_process(false)
		return
	simulation = ClassDB.instantiate("FoundationSimulation")
	_reset_demo()

func _physics_process(delta: float) -> void:
	if not running:
		return
	cadence_accumulator += delta
	var interval := 1.0 / STEPS_PER_SECOND
	while cadence_accumulator >= interval:
		cadence_accumulator -= interval
		if not _advance(1):
			running = false
			run_button.text = "Run"
			break

func _reset_demo() -> void:
	running = false
	run_button.text = "Run"
	cadence_accumulator = 0.0
	event_lines.clear()
	var result: int = simulation.reset_demo()
	_show_result(result)
	_synchronize()

func _advance(count: int) -> bool:
	var result: int = simulation.step_many(count)
	_show_result(result)
	if result != 0:
		return false
	var events: Array = simulation.get_events()
	if simulation.has_error():
		status_label.text = "Status: %s" % simulation.get_last_error()
		return false
	_append_events(events)
	return _synchronize()

func _synchronize() -> bool:
	simulation.clear_error()
	var entities: Array = simulation.get_entities()
	var resources: Array = simulation.get_resources()
	var power_edges: Array = simulation.get_power_edges()
	var tick: int = simulation.get_tick()
	var day: int = simulation.get_day()
	var time_of_day: int = simulation.get_time_of_day()
	if simulation.has_error():
		status_label.text = "Status: %s" % simulation.get_last_error()
		return false
	canvas.synchronize(entities, resources, power_edges)
	tick_label.text = "Tick: %d  Day: %d  Time: %d" % [
		tick, day, time_of_day
	]
	event_log.text = "\n".join(event_lines)
	return true

func _append_events(events: Array) -> void:
	for event: Dictionary in events:
		event_lines.append(
			"t%-4d type=%d entity=%d related=%d item=%d qty=%d" % [
				int(event.tick), int(event.type), int(event.entity_id),
				int(event.related_entity_id), int(event.item_type),
				int(event.quantity)
			]
		)
	while event_lines.size() > MAX_EVENT_LINES:
		event_lines.pop_front()

func _show_result(result: int) -> void:
	status_label.text = "Status: %s" % simulation.result_name(result)

func _on_reset_pressed() -> void:
	_reset_demo()

func _on_step_pressed() -> void:
	_advance(1)

func _on_run_pressed() -> void:
	running = not running
	run_button.text = "Pause" if running else "Run"
