extends SceneTree
## Headless tests of the app scene's controls (M7c). Buttons are pressed through their
## signals, as a click would. Run from ctest:
##   godot --headless --path godot -s res://tests/ui_tests.gd
## Every failure prints a line starting with "FAIL".

var failures := 0
var main: Control


func _initialize() -> void:
	main = load("res://main.tscn").instantiate()
	root.add_child(main)
	run.call_deferred()


func check(cond: bool, what: String) -> void:
	if not cond:
		failures += 1
		print("FAIL: " + what)


func frames(n: int) -> void:
	for i in n:
		await process_frame


func node(unique_name: String) -> Node:
	return main.get_node("%" + unique_name)


func run() -> void:
	await frames(2)
	var world: ProtodishWorld = main.world
	check(world.get_seed() == 1 and main.running, "starts running on seed 1")

	# Speed: 4 ticks per frame by default; 1 tick every 8 frames at the slowest.
	var t0 := world.get_tick()
	await frames(10)
	check(world.get_tick() - t0 == 40, "4 ticks per frame (%d in 10 frames)" % (world.get_tick() - t0))
	node("Speed").value = -3
	t0 = world.get_tick()
	await frames(16)
	check(world.get_tick() - t0 == 2, "1 tick every 8 frames (%d in 16 frames)" % (world.get_tick() - t0))
	check(node("SpeedValue").text == "1 tick every 8 frames", "speed label")
	node("Speed").value = 2

	# Start / Pause.
	node("StartPause").pressed.emit()
	t0 = world.get_tick()
	await frames(5)
	check(world.get_tick() == t0 and node("StartPause").text == "Start", "Pause stops the world")
	node("StartPause").pressed.emit()
	await frames(2)
	check(world.get_tick() > t0, "Start runs it again")

	# Seed field and Restart.
	node("Seed").text = "42"
	node("Seed").text_submitted.emit("42")
	await frames(1)
	check(world.get_seed() == 42 and world.get_tick() <= 4, "Enter in the seed field restarts with that seed")
	node("Seed").text = "not a number"
	node("Restart").pressed.emit()
	check(world.get_seed() == 42 and node("Seed").text == "42", "an invalid seed keeps the current one")
	node("NewSeed").pressed.emit()
	check(world.get_seed() != 42 and node("Seed").text == str(world.get_seed()), "New seed restarts with a new seed")

	# Presets: changing one restarts the world with the same seed.
	var preset: OptionButton = node("Preset")
	check(preset.item_count >= 2 and preset.get_item_text(0) == "Default (RULES.md)", "Default preset is first")
	var seed := world.get_seed()
	await frames(3)
	preset.select(1)
	preset.item_selected.emit(1)
	check(preset.get_item_text(1) == "Stable (climate belts, D1a)", "Stable preset is listed")
	check(world.get_tick() == 0 and world.get_seed() == seed, "changing preset restarts with the same seed")
	var stable := ProtodishWorld.new()
	stable.reset(seed, FileAccess.get_file_as_string("res://presets/stable_d1a.params"))
	check(world.get_state_hash() == stable.get_state_hash(), "the Stable preset is loaded")

	# View menu and ground layers.
	var view_menu: OptionButton = node("View")
	var ground: OptionButton = node("GroundLayer")
	check(view_menu.item_count == 5 and not ground.visible, "five views; the layer menu is hidden")
	view_menu.select(4)
	view_menu.item_selected.emit(4)
	check(ground.visible and main._view_mode() == ProtodishWorld.VIEW_GROUND, "Ground shows the layer menu")
	ground.select(3)
	ground.item_selected.emit(3)
	check(main._view_mode() == ProtodishWorld.VIEW_GROUND_MINERALS, "the layer menu picks minerals")
	check(node("Legend").get_child_count() == world.get_legend(main._view_mode()).size(), "legend follows the view")
	view_menu.select(1)
	view_menu.item_selected.emit(1)
	check(not ground.visible and main._view_mode() == ProtodishWorld.VIEW_ENERGY, "Energy hides the layer menu")

	# Statistics panel and graph.
	var StatsPanel := load("res://stats_panel.gd")
	check(StatsPanel.season_name(0, 2000) == "spring" and StatsPanel.season_name(250, 2000) == "summer"
		and StatsPanel.season_name(1249, 2000) == "autumn" and StatsPanel.season_name(1500, 2000) == "winter"
		and StatsPanel.season_name(1750, 2000) == "spring", "season names centered on midsummer and midwinter")
	await frames(30)
	var stats: Node = node("Stats")
	main._update_stats()
	check(stats.values["tick"].text == stats.Graph._thousands(world.get_tick()), "stats show the current tick")
	check(stats.values["cells"].text == stats.Graph._thousands(world.get_cell_count()), "stats show the cell count")
	check(stats.values["lineages"].text.contains("with ≥ 10 cells"), "lineages show both counts")
	check(stats.graph.history["ticks"].size() > 1, "the graph has points")
	check(stats.Graph._thousands(1234567) == "1,234,567" and stats.Graph._thousands(12) == "12", "thousands")
	check(stats.Graph._nice_ceiling(1830) == 2000 and stats.Graph._nice_ceiling(4100) == 5000, "graph scale")

	# Graph window: the last 5 years unless Whole run is on.
	check(not stats.whole_run.button_pressed, "the graph starts on the last 5 years")
	check(stats.graph_from_tick(12000, 2000) == 2000 and stats.graph_from_tick(3000, 2000) == 0, "5-year window")
	stats.whole_run.button_pressed = true
	check(stats.graph_from_tick(12000, 2000) == 0, "Whole run shows from tick 0")
	stats.whole_run.button_pressed = false

	# Click-to-inspect.
	var view: Control = main.get_node("Layout/WorldView")
	check(view.zoom >= 1, "the view has a zoom")
	check(view.site_at(view.origin + Vector2(view.zoom * 5, view.zoom * 3) + Vector2(0.5, 0.5)) == 3 * 128 + 5,
		"a point maps to its site")
	check(view.site_at(view.origin - Vector2(1, 1)) == -1, "outside the grid is no site")
	var site := -1
	for k in 128 * 128:
		if world.inspect_site(k)["occupied"]:
			site = k
			break
	node("StartPause").pressed.emit()  # pause, so the cell stays put
	view.site_clicked.emit(site)
	var tabs: TabContainer = node("RightTabs")
	var inspector: Node = node("Inspector")
	check(tabs.current_tab == 1, "a click opens the Cell tab")
	check(inspector.title.text == "Cell #%d" % world.inspect_cell()["id"], "the Cell tab shows the clicked cell")
	check(inspector.cell_box.visible and inspector.gene_values[2].text != "", "genes are shown")
	await frames(1)
	check(view.selected_site == site, "the view outlines the selected cell")
	var esc := InputEventKey.new()
	esc.keycode = KEY_ESCAPE
	esc.pressed = true
	main._unhandled_input(esc)
	check(world.inspect_cell()["state"] == "none" and inspector.title.text == "No cell selected", "Esc clears")
	var empty := site + 1 if not world.inspect_site(site + 1)["occupied"] else site - 1
	view.site_clicked.emit(empty)
	check(inspector.title.text == "Empty site" and inspector.values["minerals"].text != "–", "an empty site shows its ground")
	view.site_clicked.emit(site)
	node("StartPause").pressed.emit()

	# End world.
	await frames(3)
	for k in 128 * 128:  # select a cell that is alive now, so End world removes it
		if world.inspect_site(k)["occupied"]:
			view.site_clicked.emit(k)
			break
	node("EndWorld").pressed.emit()
	var ended_at := world.get_tick()
	await frames(3)
	check(world.is_ended() and world.get_cell_count() == 0, "End world clears every cell")
	check(world.get_tick() == ended_at and not main.running, "an ended world stays paused")
	check(node("StartPause").disabled and node("EndWorld").disabled, "Start and End world are disabled")
	check(node("Status").text.contains("World ended at tick %d" % ended_at), "status says the world ended")
	check(stats.values["cells"].text == "0", "stats update when the world ends")
	check(inspector.status.text.begins_with("Removed when the world ended"), "the inspector says the cell was removed")
	node("StartPause").pressed.emit()
	check(not main.running, "Start does nothing on an ended world")
	node("Restart").pressed.emit()
	await frames(2)
	check(not world.is_ended() and world.get_cell_count() > 0 and main.running, "Restart begins again")
	check(not node("StartPause").disabled, "Start is enabled again")

	if failures == 0:
		print("all ui tests passed")
	else:
		print("%d ui test(s) failed" % failures)
	quit(1 if failures > 0 else 0)
