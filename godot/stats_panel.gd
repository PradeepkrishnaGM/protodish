extends VBoxContainer
## Statistics panel (RULES.md, "Statistics"): live numbers in three groups and the
## population graph. show_stats() fills it from ProtodishWorld.get_stats().

const Graph := preload("res://graph.gd")

## Rows of each group: [key, label]. Keys are filled in show_stats().
const GROUPS := [
	["Time", [["tick", "Tick"], ["year", "Year"], ["season", "Season"], ["temperature", "Temperature"],
		["light", "Light"]]],
	["Cells", [["cells", "Cells"], ["free_body", "Free / in bodies"], ["bodies", "Bodies"],
		["lineages", "Lineages"], ["producers", "Producers"], ["consumers", "Consumers"],
		["infected", "Infected"]]],
	["Matter", [["matter_cells", "In cells"], ["matter_food", "As food (A / B)"],
		["matter_minerals", "As minerals"]]],
]
const HEADING_SIZE := 16
const MUTED := Color(0.6, 0.62, 0.65)

var values := {}  ## key -> value Label
var graph: Graph


func _ready() -> void:
	add_theme_constant_override("separation", 4)
	# One grid for every group, so the value column lines up across groups.
	var grid := GridContainer.new()
	grid.columns = 2
	grid.add_theme_constant_override("h_separation", 10)
	for group in GROUPS:
		grid.add_child(_heading(group[0]))
		grid.add_child(Control.new())
		for row in group[1]:
			var name := Label.new()
			name.text = row[1]
			name.add_theme_color_override("font_color", MUTED)
			grid.add_child(name)
			var value := Label.new()
			value.size_flags_horizontal = Control.SIZE_EXPAND_FILL
			grid.add_child(value)
			values[row[0]] = value
	add_child(grid)
	add_child(_heading("Population"))
	var key := HBoxContainer.new()
	for s in Graph.SERIES:
		var label := Label.new()
		label.text = "— " + s[0]
		label.add_theme_color_override("font_color", s[1])
		label.add_theme_font_size_override("font_size", 12)
		key.add_child(label)
	add_child(key)
	graph = Graph.new()
	graph.custom_minimum_size = Vector2(0, 170)
	graph.size_flags_vertical = Control.SIZE_EXPAND_FILL
	add_child(graph)


func show_stats(s: Dictionary, history: Dictionary) -> void:
	var tick: int = s["tick"]
	var year_length: int = s["year_length"]
	var cells: int = s["cells"]
	var season: float = s["season"]
	values["tick"].text = Graph._thousands(tick)
	values["year"].text = str(tick / year_length + 1)
	values["season"].text = "%s (%+.2f)" % [season_name(tick, year_length), season]
	values["temperature"].text = "%.1f – %.1f °C" % [s["temp_min"], s["temp_max"]]
	values["light"].text = "%.2f – %.2f" % [s["light_min"], s["light_max"]]
	values["cells"].text = Graph._thousands(cells)
	values["free_body"].text = "%s / %s" % [Graph._thousands(s["free_cells"]), Graph._thousands(s["body_cells"])]
	values["bodies"].text = "%s (largest %d)" % [Graph._thousands(s["bodies"]), s["largest_body"]] \
		if s["bodies"] > 0 else "0"
	values["lineages"].text = "%d (%d with ≥ %d cells)" % [s["clusters"], s["clusters_large"], s["cluster_min_size"]]
	values["producers"].text = _share(s["producers"], cells)
	values["consumers"].text = _share(s["consumers"], cells)
	values["infected"].text = _share(s["infected"], cells)
	var total: float = s["matter_total"]
	values["matter_cells"].text = _matter(s["matter_cells"], total)
	values["matter_food"].text = "%s\n(%s / %s)" % [_matter(s["matter_food_a"] + s["matter_food_b"], total),
		Graph._thousands(roundi(s["matter_food_a"])), Graph._thousands(roundi(s["matter_food_b"]))]
	values["matter_minerals"].text = _matter(s["matter_minerals"], total)
	graph.set_data(history, year_length, tick)


## Season name of a tick. RULES.md puts midsummer (season +1) at tick 500 of the year and
## midwinter (-1) at 1500, so each season is the quarter of the year centered on its peak:
## spring 1750-249, summer 250-749, autumn 750-1249, winter 1250-1749 (for 2000-tick years).
static func season_name(tick: int, year_length: int) -> String:
	var quarter := ((tick % year_length) * 4 + year_length / 2) / year_length % 4
	return ["spring", "summer", "autumn", "winter"][quarter]


static func _share(n: int, of: int) -> String:
	return "%s (%d%%)" % [Graph._thousands(n), roundi(100.0 * n / of)] if of > 0 else Graph._thousands(n)


static func _matter(amount: float, total: float) -> String:
	return "%s (%.1f%%)" % [Graph._thousands(roundi(amount)), 100.0 * amount / total] if total > 0 else "0"


func _heading(text: String) -> Label:
	var label := Label.new()
	label.text = text
	label.add_theme_font_size_override("font_size", HEADING_SIZE)
	return label
