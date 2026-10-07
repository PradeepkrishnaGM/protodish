extends Control
## M7a: the world view only. The world runs at a fixed number of ticks per frame;
## Space pauses and resumes.

const TICKS_PER_FRAME := 4

var world := ProtodishWorld.new()
var running := true
var texture: ImageTexture

@onready var view: TextureRect = $WorldView
@onready var status: Label = $Status


func _ready() -> void:
	world.reset(1)
	texture = ImageTexture.create_from_image(world.render(ProtodishWorld.VIEW_LINEAGE))
	view.texture = texture


func _process(_delta: float) -> void:
	if running and not world.is_extinct():
		world.step(TICKS_PER_FRAME)
	texture.update(world.render(ProtodishWorld.VIEW_LINEAGE))
	var state := "paused" if not running else ("extinct" if world.is_extinct() else "running")
	status.text = "tick %d   cells %d   %s   (Space: pause)" % [world.get_tick(), world.get_cell_count(), state]


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and not event.echo and event.keycode == KEY_SPACE:
		running = not running
