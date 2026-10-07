extends Control
## The app: controls on the left, the world view in the middle, statistics on the right
## (RULES.md, "The application"). Clicking the world view inspects a cell or site.

const PRESET_DIR := "res://presets"
const DEFAULT_PRESET := "default.params"  ## listed first
const DEFAULT_SEED := 1
const MAIN_VIEWS := 5  ## Lineage, Energy, Feeding type, Infection, Ground
const GROUND_LAYERS := ["All layers", "Food A", "Food B", "Minerals"]
# Preloaded rather than class_name, which needs the editor's class cache to resolve.
const WorldView := preload("res://world_view.gd")
const Legend := preload("res://legend.gd")
const StatsPanel := preload("res://stats_panel.gd")
const Inspector := preload("res://inspector.gd")
const CELL_TAB := 1
const STATS_INTERVAL := 0.2  ## seconds between statistics updates

var world := ProtodishWorld.new()
var running := true
var frame := 0
var since_stats := 0.0
var texture: ImageTexture
var strip_texture: ImageTexture
var inspected_site := -1  ## an empty site that was clicked; -1 when a cell is selected or nothing
var presets: Array[Dictionary] = []  ## {name, text}
var preset_index := 0
var preset_error := ""  ## shown until the next successful restart

@onready var view: WorldView = $Layout/WorldView
@onready var status: Label = %Status
@onready var start_pause: Button = %StartPause
@onready var speed: HSlider = %Speed
@onready var speed_value: Label = %SpeedValue
@onready var seed_edit: LineEdit = %Seed
@onready var preset_menu: OptionButton = %Preset
@onready var view_menu: OptionButton = %View
@onready var ground_menu: OptionButton = %GroundLayer
@onready var legend: Legend = %Legend
@onready var stats: StatsPanel = %Stats
@onready var inspector: Inspector = %Inspector
@onready var right_tabs: TabContainer = %RightTabs


func _ready() -> void:
	var names := world.get_view_mode_names()
	for m in MAIN_VIEWS:
		view_menu.add_item(names[m], m)
	for layer in GROUND_LAYERS:
		ground_menu.add_item(layer)
	_load_presets()
	for p in presets:
		preset_menu.add_item(p["name"])

	start_pause.pressed.connect(_toggle_running)
	speed.value_changed.connect(func(_v: float) -> void: _update_speed_label())
	%Restart.pressed.connect(func() -> void: _restart(_seed_from_field()))
	%EndWorld.pressed.connect(_end_world)
	seed_edit.text_submitted.connect(func(_t: String) -> void: _restart(_seed_from_field()))
	%NewSeed.pressed.connect(func() -> void: _restart(randi_range(1, 999_999_999)))
	preset_menu.item_selected.connect(func(i: int) -> void:
		preset_index = i
		_restart(world.get_seed()))
	view_menu.item_selected.connect(func(_i: int) -> void: _update_view())
	ground_menu.item_selected.connect(func(_i: int) -> void: _update_view())
	view.site_clicked.connect(_on_site_clicked)
	inspector.clear_requested.connect(_clear_selection)
	stats.graph_range_changed.connect(_update_stats)

	_restart(DEFAULT_SEED)
	_update_speed_label()
	_update_view()


## Reads every *.params file in PRESET_DIR. Its "# name: ..." line is the menu label.
func _load_presets() -> void:
	var files := Array(DirAccess.get_files_at(PRESET_DIR)).filter(
		func(f: String) -> bool: return f.ends_with(".params"))
	files.sort_custom(func(a: String, b: String) -> bool:
		return a == DEFAULT_PRESET or (b != DEFAULT_PRESET and a < b))
	for f: String in files:
		var text := FileAccess.get_file_as_string(PRESET_DIR.path_join(f))
		var name := f.get_basename()
		for line in text.split("\n"):
			if line.begins_with("# name:"):
				name = line.trim_prefix("# name:").strip_edges()
				break
		presets.append({"name": name, "text": text})


func _process(delta: float) -> void:
	frame += 1
	if running:
		var k := int(speed.value)
		var ticks := 1 << k if k >= 0 else (1 if frame % (1 << -k) == 0 else 0)
		if ticks > 0:
			world.step(ticks)
		if world.is_extinct():
			running = false
	texture.update(world.render(_view_mode()))
	strip_texture.update(world.render_temperature_strip())
	view.selected_site = world.get_selected_site()
	_update_status()
	since_stats += delta
	if since_stats >= STATS_INTERVAL:
		_update_stats()


func _update_stats() -> void:
	since_stats = 0.0
	var s := world.get_stats()
	var from_tick := stats.graph_from_tick(s["tick"], s["year_length"])
	stats.show_stats(s, world.get_history_since(stats.graph.max_points(), from_tick))
	_update_inspector()


func _update_inspector() -> void:
	var cell := world.inspect_cell()
	if cell["state"] != "none":
		inspector.show_cell(cell, world.inspect_site(cell["site"]), world.get_tick())
	elif inspected_site >= 0:
		inspector.show_cell(cell, world.inspect_site(inspected_site), world.get_tick())
	else:
		inspector.show_nothing()


## Selects the cell on a clicked site, or shows the site's ground if it is empty.
func _on_site_clicked(site: int) -> void:
	if site < 0:
		return
	inspected_site = -1 if world.select_site(site) else site
	view.selected_site = world.get_selected_site()
	right_tabs.current_tab = CELL_TAB
	_update_inspector()


func _clear_selection() -> void:
	world.clear_selection()
	inspected_site = -1
	view.selected_site = -1
	_update_inspector()


func _unhandled_input(event: InputEvent) -> void:
	if not (event is InputEventKey and event.pressed and not event.echo):
		return
	if event.keycode == KEY_SPACE:
		_toggle_running()
	elif event.keycode == KEY_ESCAPE:
		_clear_selection()
	elif event.keycode >= KEY_1 and event.keycode < KEY_1 + MAIN_VIEWS:
		view_menu.select(event.keycode - KEY_1)
		_update_view()
	else:
		return
	get_viewport().set_input_as_handled()


func _restart(seed: int) -> void:
	var err := world.reset(seed, presets[preset_index]["text"] if presets.size() > 0 else "")
	preset_error = err
	if err != "":
		push_error("preset %s: %s" % [presets[preset_index]["name"], err])
	seed_edit.text = str(world.get_seed())
	preset_menu.select(preset_index)
	texture = ImageTexture.create_from_image(world.render(_view_mode()))
	view.texture = texture
	strip_texture = ImageTexture.create_from_image(world.render_temperature_strip())
	view.strip = strip_texture
	inspected_site = -1  # reset() clears the selection
	view.selected_site = -1
	running = true
	_update_status()
	_update_stats()


func _end_world() -> void:
	world.end_world()
	running = false
	_update_status()
	_update_stats()


func _toggle_running() -> void:
	if world.is_extinct() or world.is_ended():
		return
	running = not running
	_update_status()


## The seed typed in the field, or the current seed if the field does not hold one.
func _seed_from_field() -> int:
	var text := seed_edit.text.strip_edges()
	return int(text) if text.is_valid_int() and int(text) >= 0 else world.get_seed()


func _view_mode() -> int:
	var main := view_menu.get_selected_id()
	return main if main < ProtodishWorld.VIEW_GROUND else ProtodishWorld.VIEW_GROUND + ground_menu.selected


func _update_view() -> void:
	ground_menu.visible = view_menu.get_selected_id() == ProtodishWorld.VIEW_GROUND
	legend.show_items(world.get_legend(_view_mode()))


func _update_speed_label() -> void:
	var k := int(speed.value)
	speed_value.text = ("%d tick%s per frame" % [1 << k, "" if k == 0 else "s"]) if k >= 0 \
		else "1 tick every %d frames" % (1 << -k)


func _update_status() -> void:
	# Tick and cell counts are in the statistics panel.
	var line := "Running" if running else "Paused"
	if world.is_ended():
		line = "World ended at tick %d. Restart to begin again." % world.get_ended_at()
	elif world.is_extinct():
		line = "Extinct: the last cell died at tick %d. Restart to begin again." % world.get_extinct_at()
	if preset_error != "":
		line += "\nPreset error: " + preset_error
	status.text = line
	start_pause.text = "Pause" if running else "Start"
	start_pause.disabled = world.is_extinct() or world.is_ended()
	%EndWorld.disabled = world.is_ended()
