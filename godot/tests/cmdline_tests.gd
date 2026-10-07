extends SceneTree
## Headless test of the app's command-line options (M8). Run from ctest:
##   godot --headless --path godot -s res://tests/cmdline_tests.gd -- --seed 42 --preset stable_d1a --view 3 --speed 16
## Without options the app starts on seed 1 with the default preset (ui_tests.gd checks that).
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


func run() -> void:
	await process_frame
	check(main.world.get_seed() == 42, "--seed 42 (got %d)" % main.world.get_seed())
	check(main.presets[main.preset_index]["file"] == "stable_d1a.params", "--preset stable_d1a")
	check(main.preset_error == "", "the preset applies without error")
	check(main.view_menu.get_selected_id() == 2, "--view 3 selects Feeding type")
	check(int(main.speed.value) == 4, "--speed 16 sets 16 ticks per frame")
	check(main.get_node("%SpeedValue").text == "16 ticks per frame", "speed label")
	var t0: int = main.world.get_tick()
	for i in 5:
		await process_frame
	check(main.world.get_tick() - t0 == 80, "16 ticks per frame (%d in 5 frames)" % (main.world.get_tick() - t0))
	print("cmdline tests: %d failures" % failures)
	quit(1 if failures > 0 else 0)
