class_name FoundationPresentationFormat
extends RefCounted

const ENTITY_NAMES := ["None", "Extractor", "Belt", "Refinery", "Assembler", "Storage", "Splitter", "Inserter", "Power Pole", "Burner Generator", "Fluid Tank", "Pipe", "Water Extractor", "Boiler", "Steam Engine", "Solar Generator", "Accumulator", "Reactor Core", "Heat Conductor", "Heat Exchanger", "Steam Turbine", "Steam Condenser"]
const DIRECTIONS := ["North", "East", "South", "West"]
const ITEMS := ["None", "Iron Ore", "Iron Plate", "Copper Ore", "Copper Plate", "Electronic Component", "Iron Gear", "Copper Wire", "Biomass Pellet", "Basic Science"]
const FLUIDS := ["None", "Water", "Steam", "Exhaust Steam"]
const STATUSES := ["None", "Idle", "Working", "Blocked: input", "Blocked: output", "Unpowered"]

static func entity_type(value: int) -> String:
	return ENTITY_NAMES[value] if value >= 0 and value < ENTITY_NAMES.size() else "Entity type %d" % value

static func direction(value: int) -> String:
	return DIRECTIONS[value] if value >= 0 and value < DIRECTIONS.size() else "None"

static func item(value: int) -> String:
	return ITEMS[value] if value >= 0 and value < ITEMS.size() else "Item %d" % value

static func fluid(value: int) -> String:
	return FLUIDS[value] if value >= 0 and value < FLUIDS.size() else "Fluid %d" % value

static func machine_status(value: int) -> String:
	return STATUSES[value] if value >= 0 and value < STATUSES.size() else "Status %d" % value

static func number(value: int) -> String:
	var source := str(value)
	var result := ""
	while source.length() > 3:
		result = "," + source.right(3) + result
		source = source.left(-3)
	return source + result

static func yes_no(value: bool) -> String:
	return "Yes" if value else "No"
