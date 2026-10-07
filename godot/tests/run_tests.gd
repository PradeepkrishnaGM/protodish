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
	test_render_lineage()

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


## The wrapper must give the same history as the CLI for the same seed.
func test_matches_cli() -> void:
	if evolve_path == "":
		print("SKIP: matches_cli (no --evolve given)")
		return
	var output := []
	var code := OS.execute(evolve_path, ["--seed", "1", "--ticks", "1000", "--every", "1000"], output)
	check(code == 0, "evolve ran (exit %d)" % code)
	var cli_hash := ""
	for line in String(output[0]).split("\n"):
		var cols := line.split(",")
		if cols.size() > 1 and cols[0] == "1000":
			cli_hash = cols[cols.size() - 1]
	var w := ProtodishWorld.new()
	w.reset(1)
	var ran := w.step(1000)
	check(ran == 1000 and w.get_tick() == 1000, "stepped 1000 ticks (ran %d)" % ran)
	check(cli_hash != "" and w.get_state_hash() == cli_hash,
		"hash after 1000 ticks matches the CLI (%s vs %s)" % [w.get_state_hash(), cli_hash])


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


func test_render_lineage() -> void:
	var w := ProtodishWorld.new()
	w.reset(3)
	w.step(300)
	var img := w.render(ProtodishWorld.VIEW_LINEAGE)
	check(img.get_width() == 128 and img.get_height() == 128, "image is 128 x 128")
	check(img.get_format() == Image.FORMAT_RGB8, "image is RGB8")
	var occupied := 0
	for y in img.get_height():
		for x in img.get_width():
			if img.get_pixel(x, y) != EMPTY:
				occupied += 1
	check(occupied == w.get_cell_count(),
		"one colored pixel per cell (%d pixels, %d cells)" % [occupied, w.get_cell_count()])
	save_snapshot(img, "lineage")


## Saves the image scaled up 4x without smoothing, for viewing.
func save_snapshot(img: Image, name: String) -> void:
	if out_dir == "":
		return
	DirAccess.make_dir_recursive_absolute(out_dir)
	var big := img.duplicate() as Image
	big.resize(img.get_width() * 4, img.get_height() * 4, Image.INTERPOLATE_NEAREST)
	var path := out_dir.path_join("view_%s.png" % name)
	check(big.save_png(path) == OK, "saved " + path)
