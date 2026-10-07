extends SceneTree
## Headless tests for the GDExtension wrapper (M7). Run from ctest:
##   godot --headless --path godot -s res://tests/run_tests.gd -- --evolve PATH --out DIR
## Paths must be absolute, because Godot runs from the project folder. --evolve is the CLI
## binary, used to check that the wrapper runs the same simulation. --out is where snapshot
## PNGs go. Every failure prints a line starting with "FAIL".

const EMPTY := Color8(16, 20, 24)

var failures := 0
var evolve_path := ""
var out_dir := ""


func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	for i in args.size() - 1:
		if args[i] == "--evolve":
			evolve_path = args[i + 1]
		elif args[i] == "--out":
			out_dir = args[i + 1]

	test_class_exists()
	test_matches_cli()
	test_determinism()
	test_render_modes()
	test_presets()
	test_bad_params()
	test_end_world()
	test_legends()

	if failures == 0:
		print("all godot tests passed")
	else:
		print("%d godot test(s) failed" % failures)
	quit(1 if failures > 0 else 0)


func check(cond: bool, what: String) -> void:
	if not cond:
		failures += 1
		print("FAIL: " + what)


func run_hash(seed: int, ticks: int) -> String:
	var w := ProtodishWorld.new()
	w.reset(seed)
	w.step(ticks)
	return w.get_state_hash()


func test_class_exists() -> void:
	check(ClassDB.class_exists("ProtodishWorld"), "ProtodishWorld is registered")


## Hash of the CLI's census row at `tick`, or "" if the CLI did not run.
func cli_hash(args: PackedStringArray, tick: int) -> String:
	var output := []
	var code := OS.execute(evolve_path, args + PackedStringArray(["--every", str(tick)]), output)
	check(code == 0, "evolve %s ran (exit %d)" % [" ".join(args), code])
	for line in String(output[0]).split("\n"):
		var cols := line.split(",")
		if cols.size() > 1 and cols[0] == str(tick):
			return cols[cols.size() - 1]
	return ""


## The wrapper must give the same history as the CLI for the same seed.
func test_matches_cli() -> void:
	if evolve_path == "":
		print("SKIP: matches_cli (no --evolve given)")
		return
	var expected := cli_hash(["--seed", "1", "--ticks", "1000"], 1000)
	var w := ProtodishWorld.new()
	w.reset(1)
	var ran := w.step(1000)
	check(ran == 1000 and w.get_tick() == 1000, "stepped 1000 ticks (ran %d)" % ran)
	check(expected != "" and w.get_state_hash() == expected,
		"hash after 1000 ticks matches the CLI (%s vs %s)" % [w.get_state_hash(), expected])


## Every preset loads, and a preset gives the same history as the CLI with that file.
func test_presets() -> void:
	var files := Array(DirAccess.get_files_at("res://presets")).filter(
		func(f: String) -> bool: return f.ends_with(".params"))
	check(files.has("default.params") and files.has("stable_d1a.params"), "both presets exist")
	var w := ProtodishWorld.new()
	for f: String in files:
		var err := w.reset(1, FileAccess.get_file_as_string("res://presets/" + f))
		check(err == "", "preset %s loads (%s)" % [f, err])
	w.reset(1, FileAccess.get_file_as_string("res://presets/default.params"))
	check(w.get_state_hash() == run_hash(1, 0), "the default preset is the RULES.md world")
	if evolve_path == "":
		return
	var path := ProjectSettings.globalize_path("res://presets/stable_d1a.params")
	var expected := cli_hash(["--seed", "1", "--ticks", "1000", "--params", path], 1000)
	w.reset(1, FileAccess.get_file_as_string(path))
	w.step(1000)
	check(expected != "" and w.get_state_hash() == expected,
		"Stable preset matches the CLI with the same file (%s vs %s)" % [w.get_state_hash(), expected])
	check(w.get_state_hash() != run_hash(1, 1000), "the Stable preset changes the history")


## A bad preset is reported and the current world is kept.
func test_bad_params() -> void:
	var w := ProtodishWorld.new()
	w.reset(5)
	w.step(20)
	var before := w.get_state_hash()
	for text in ["no_such_key = 1", "temp_base = hot", "clone_cost = 3"]:
		var err := w.reset(9, text)
		check(err != "", "rejected: " + text)
		check(w.get_state_hash() == before and w.get_seed() == 5, "world kept after: " + text)


func test_end_world() -> void:
	var w := ProtodishWorld.new()
	w.reset(2)
	w.step(400)
	var matter := w.get_total_matter()
	var tick := w.get_tick()
	w.end_world()
	check(w.is_ended() and w.get_ended_at() == tick, "end_world marks the world ended at this tick")
	check(w.get_cell_count() == 0, "end_world clears every cell")
	check(absf(w.get_total_matter() - matter) <= 1e-9 * matter, "end_world conserves matter")
	check(not w.is_extinct(), "an ended world is not extinct")
	check(w.step(10) == 0 and w.get_tick() == tick, "an ended world does not step")
	check(count_non_empty(w.render(ProtodishWorld.VIEW_LINEAGE)) == 0, "no cells are drawn")
	w.reset(2)
	check(not w.is_ended() and w.get_cell_count() == 50, "reset starts a fresh world")


func test_legends() -> void:
	var w := ProtodishWorld.new()
	for mode in ProtodishWorld.VIEW_MODE_COUNT:
		var items := w.get_legend(mode)
		check(items.size() > 0, "mode %d has a legend" % mode)
		for item: Dictionary in items:
			check(item.has_all(["label", "colors", "low", "high"]), "legend item has every key")
			check((item["colors"] as PackedColorArray).size() > 0, "legend item has colors")


func test_determinism() -> void:
	var a := run_hash(7, 500)
	check(a == run_hash(7, 500), "same seed gives the same hash")
	check(a != run_hash(8, 500), "different seeds give different hashes")
	var w := ProtodishWorld.new()
	w.reset(7)
	w.step(200)
	w.reset(7)
	w.step(500)
	check(w.get_state_hash() == a, "reset with the same seed repeats the history")


func test_render_modes() -> void:
	var w := ProtodishWorld.new()
	w.reset(3)
	w.step(300)
	var names := w.get_view_mode_names()
	check(names.size() == ProtodishWorld.VIEW_MODE_COUNT, "one name per view mode")
	for mode in ProtodishWorld.VIEW_MODE_COUNT:
		var img := w.render(mode)
		check(img.get_width() == 128 and img.get_height() == 128, "%s: image is 128 x 128" % names[mode])
		check(img.get_format() == Image.FORMAT_RGB8, "%s: image is RGB8" % names[mode])
		var occupied := count_non_empty(img)
		if mode >= ProtodishWorld.VIEW_GROUND:
			check(occupied > w.get_cell_count(), "%s: the ground is drawn on empty sites too" % names[mode])
		else:
			check(occupied == w.get_cell_count(),
				"%s: one colored pixel per cell (%d pixels, %d cells)" % [names[mode], occupied, w.get_cell_count()])
		save_snapshot(img, names[mode].replace(":", "").to_snake_case())
	var hash_before := w.get_state_hash()
	w.render(ProtodishWorld.VIEW_GROUND)
	check(w.get_state_hash() == hash_before, "rendering does not change the world")
	check(count_non_empty(w.render(99)) == w.get_cell_count(), "an unknown mode paints lineage")


func count_non_empty(img: Image) -> int:
	var n := 0
	for y in img.get_height():
		for x in img.get_width():
			if img.get_pixel(x, y) != EMPTY:
				n += 1
	return n


## Saves the image scaled up 4x without smoothing, for viewing.
func save_snapshot(img: Image, name: String) -> void:
	if out_dir == "":
		return
	DirAccess.make_dir_recursive_absolute(out_dir)
	var big := img.duplicate() as Image
	big.resize(img.get_width() * 4, img.get_height() * 4, Image.INTERPOLATE_NEAREST)
	var path := out_dir.path_join("view_%s.png" % name)
	check(big.save_png(path) == OK, "saved " + path)
