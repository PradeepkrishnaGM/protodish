extends Control
## M7b: the world view only. The world runs at a fixed number of ticks per frame;
## Space pauses and resumes, keys 1-5 choose the view mode.

const TICKS_PER_FRAME := 4

var world := ProtodishWorld.new()
var running := true
var view_mode := ProtodishWorld.VIEW_LINEAGE
var mode_names := PackedStringArray()
var texture: ImageTexture

@onready var view: TextureRect = $WorldView
@onready var status: Label = $Status


func _ready() -> void:
	world.reset(1)
	mode_names = world.get_view_mode_names()
	texture = ImageTexture.create_from_image(world.render(view_mode))
	view.texture = texture


func _process(_delta: float) -> void:
	if running and not world.is_extinct():
		world.step(TICKS_PER_FRAME)
	texture.update(world.render(view_mode))
	var state := "paused" if not running else ("extinct" if world.is_extinct() else "running")
	status.text = "tick %d   cells %d   %s   view: %s   (Space: pause, 1-5: view)" % [
		world.get_tick(), world.get_cell_count(), state, mode_names[view_mode]]


func _unhandled_input(event: InputEvent) -> void:
	if not (event is InputEventKey and event.pressed and not event.echo):
		return
	if event.keycode == KEY_SPACE:
		running = not running
	elif event.keycode >= KEY_1 and event.keycode < KEY_1 + ProtodishWorld.VIEW_MODE_COUNT:
		view_mode = event.keycode - KEY_1
