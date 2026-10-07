extends SceneTree
## Saves a screenshot of the main scene after some frames. Needs a display (Xvfb works):
##   xvfb-run -s "-screen 0 1280x800x24" godot --path godot -s res://tests/screenshot.gd \
##       -- --out /abs/shot.png [--frames N] [--keys 3]
## --keys presses each listed key (one character each) halfway through, in order.

var out_path := ""
var frames := 120
var keys := ""
var press_at := 0
var pressed := false


func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	for i in args.size() - 1:
		if args[i] == "--out":
			out_path = args[i + 1]
		elif args[i] == "--frames":
			frames = int(args[i + 1])
		elif args[i] == "--keys":
			keys = args[i + 1]
	press_at = frames / 2
	root.add_child(load("res://main.tscn").instantiate())


func _process(_delta: float) -> bool:
	frames -= 1
	if not pressed and frames <= press_at:
		pressed = true
		for ch in keys:
			for down in [true, false]:
				var ev := InputEventKey.new()
				ev.keycode = OS.find_keycode_from_string(ch)
				ev.pressed = down
				Input.parse_input_event(ev)
	if frames > 0:
		return false
	var img := root.get_texture().get_image()
	print("renderer: %s, %s" % [RenderingServer.get_current_rendering_method(), RenderingServer.get_video_adapter_name()])
	print("saved %s: %s" % [out_path, error_string(img.save_png(out_path))])
	return true
