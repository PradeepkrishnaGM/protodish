extends VBoxContainer
## Cell tab of the right panel (RULES.md: clicking a cell shows its genes, stores, age and
## stress). Shows the selected cell, followed by ID, or the ground of an empty site.

signal clear_requested

## RULES.md genome table names, in gene order.
const GENE_NAMES := ["Tag", "Tolerance", "Harvest", "Diet", "Photosynthesis", "Preferred temperature",
	"Attack", "Defense", "Resistance", "Adhesion", "Share", "Role split", "Motility", "Appetite",
	"Caution", "Boldness", "Sociability", "Dormancy", "Mutability", "Mating"]
const CELL_ROWS := [["stores", "Stores A / B"], ["age", "Age"], ["stress", "Stress"],
	["activity", "Activity"], ["cooldown", "Cooldown"], ["infection", "Infection"],
	["body", "Body"], ["feeding", "Feeding type"], ["thermal", "Thermal efficiency"],
	["supply", "Supply"], ["parents", "Parents"]]
const SITE_ROWS := [["position", "Row, column"], ["temperature", "Temperature"], ["light", "Light"],
	["food", "Food A / B"], ["minerals", "Minerals"]]
const MUTED := Color(0.6, 0.62, 0.65)
const BAR_SIZE := Vector2(56, 8)

var title: Label
var status: Label
var clear_button: Button
var cell_box: Control
var values := {}  ## key -> value Label
var gene_values: Array[Label] = []
var gene_bars: Array[ProgressBar] = []


func _ready() -> void:
	add_theme_constant_override("separation", 6)
	var head := HBoxContainer.new()
	title = Label.new()
	title.add_theme_font_size_override("font_size", 16)
	title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	head.add_child(title)
	clear_button = Button.new()
	clear_button.text = "Clear"
	clear_button.pressed.connect(func() -> void: clear_requested.emit())
	head.add_child(clear_button)
	add_child(head)
	status = Label.new()
	status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	add_child(status)

	cell_box = VBoxContainer.new()
	cell_box.add_child(_grid(CELL_ROWS))
	cell_box.add_child(_heading("Genes"))
	var genes := GridContainer.new()
	genes.columns = 3
	genes.add_theme_constant_override("h_separation", 8)
	for name in GENE_NAMES:
		genes.add_child(_muted(name))
		var value := Label.new()
		value.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
		value.custom_minimum_size = Vector2(44, 0)
		genes.add_child(value)
		gene_values.append(value)
		var bar := ProgressBar.new()
		bar.show_percentage = false
		bar.custom_minimum_size = BAR_SIZE
		bar.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		bar.max_value = 1.0
		bar.step = 0.0
		genes.add_child(bar)
		gene_bars.append(bar)
	cell_box.add_child(genes)
	add_child(cell_box)
	add_child(_heading("Site"))
	add_child(_grid(SITE_ROWS))
	show_nothing()


func show_nothing() -> void:
	title.text = "No cell selected"
	status.text = "Click a cell in the world view to inspect it. The selection follows the cell as it moves."
	clear_button.visible = false
	cell_box.visible = false
	for row in SITE_ROWS:
		values[row[0]].text = "–"


## cell: ProtodishWorld.inspect_cell(); site: ProtodishWorld.inspect_site() of the cell's
## site, or of the clicked site when no cell is selected.
func show_cell(cell: Dictionary, site: Dictionary, tick: int) -> void:
	clear_button.visible = true
	_show_site(site)
	match cell["state"]:
		"none":
			title.text = "Empty site"
			status.text = "No cell here."
			cell_box.visible = false
			return
		"dead":
			title.text = "Cell #%d" % cell["id"]
			status.text = "Died at tick %d (%s)." % [cell["death_tick"], cell["death_cause"]]
			cell_box.visible = false
			return
		"removed":
			title.text = "Cell #%d" % cell["id"]
			status.text = "Removed when the world ended at tick %d." % cell["death_tick"]
			cell_box.visible = false
			return
	title.text = "Cell #%d" % cell["id"]
	status.text = "Alive at tick %d." % tick
	cell_box.visible = true
	values["stores"].text = "%.1f / %.1f of %d" % [cell["store_a"], cell["store_b"], cell["store_max"]]
	values["age"].text = str(cell["age"])
	values["stress"].text = "%.2f" % cell["stress"]
	values["activity"].text = "born this tick" if cell["newborn"] else ("dormant" if cell["dormant"] else "awake")
	values["cooldown"].text = str(cell["cooldown"])
	values["infection"].text = "virus, tag %.3f" % cell["virus_tag"] if cell["infected"] else "healthy"
	var bonds: int = cell["bonds"]
	values["body"].text = "free cell" if bonds == 0 else "%d cells, %d bonds, %s" % [
		cell["body_size"], bonds, "inner" if cell["inner"] else "outer"]
	values["feeding"].text = "producer" if cell["producer"] else "consumer"
	values["thermal"].text = "%.2f" % cell["thermal_eff"]
	values["supply"].text = "%.2f" % cell["supply"]
	var parents := "ancestor" if cell["parent_id"] == 0 else "#%d" % cell["parent_id"]
	if cell["parent2_id"] != 0:
		parents += " and #%d (mating)" % cell["parent2_id"]
	values["parents"].text = parents
	var genes: Array = cell["genes"]
	for k in genes.size():
		var g: Dictionary = genes[k]
		var v: float = g["value"]
		gene_values[k].text = "%.1f" % v if g["max"] > 1.0 else "%.3f" % v
		gene_bars[k].value = (v - g["min"]) / (g["max"] - g["min"])


func _show_site(site: Dictionary) -> void:
	if site.is_empty():
		return
	values["position"].text = "%d, %d" % [site["row"], site["col"]]
	values["temperature"].text = "%.1f °C" % site["temperature"]
	values["light"].text = "%.2f" % site["light"]
	values["food"].text = "%.2f / %.2f" % [site["food_a"], site["food_b"]]
	values["minerals"].text = "%.2f" % site["minerals"]


func _grid(rows: Array) -> GridContainer:
	var grid := GridContainer.new()
	grid.columns = 2
	grid.add_theme_constant_override("h_separation", 10)
	for row in rows:
		grid.add_child(_muted(row[1]))
		var value := Label.new()
		value.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		value.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		grid.add_child(value)
		values[row[0]] = value
	return grid


func _muted(text: String) -> Label:
	var label := Label.new()
	label.text = text
	label.add_theme_color_override("font_color", MUTED)
	return label


func _heading(text: String) -> Label:
	var label := Label.new()
	label.text = text
	label.add_theme_font_size_override("font_size", 16)
	return label
