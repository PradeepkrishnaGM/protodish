extends Control
## Draws the world image at the largest whole-number zoom that fits, centered, so every
## site is the same number of pixels. A strip on the left shows each row's temperature.
## The selected cell is outlined. A left click emits site_clicked with the site under it.

signal site_clicked(site: int)

const OUTLINE_OUTER := Color(0, 0, 0)
const OUTLINE_INNER := Color(1, 1, 1)
const OUTLINE_GAP := 2.0  ## pixels between the site and the ring

var texture: Texture2D:
	set(value):
		texture = value
		queue_redraw()
var strip: Texture2D:  ## 1 x height, one pixel per row
	set(value):
		strip = value
		queue_redraw()
var selected_site := -1:
	set(value):
		if value != selected_site:
			selected_site = value
			queue_redraw()

var zoom := 1  ## screen pixels per site
var origin := Vector2.ZERO  ## top-left corner of the grid inside this control


func _ready() -> void:
	resized.connect(queue_redraw)


## Strip width and the gap before the grid, for a zoom.
static func strip_size(z: int) -> Vector2i:
	return Vector2i(maxi(4, 2 * z), maxi(2, z))


func _draw() -> void:
	if texture == null:
		return
	var size_px := texture.get_size()
	zoom = 1
	while true:
		var s := strip_size(zoom + 1)
		if (zoom + 1) * size_px.x + s.x + s.y > size.x or (zoom + 1) * size_px.y > size.y:
			break
		zoom += 1
	var sv := strip_size(zoom)
	var shown := size_px * zoom
	var total := Vector2(shown.x + sv.x + sv.y, shown.y)
	var left := ((size - total) / 2).floor()
	origin = left + Vector2(sv.x + sv.y, 0)
	if strip != null:
		draw_texture_rect(strip, Rect2(left, Vector2(sv.x, shown.y)), false)
	draw_texture_rect(texture, Rect2(origin, shown), false)
	if selected_site >= 0:
		var w := int(size_px.x)
		var cell := origin + Vector2(selected_site % w, selected_site / w) * zoom
		# A white ring with a black edge, drawn outside the site so the cell's color stays visible.
		var box := Rect2(cell, Vector2(zoom, zoom)).grow(OUTLINE_GAP)
		draw_rect(box, OUTLINE_INNER, false, 2.0)
		draw_rect(box.grow(2), OUTLINE_OUTER, false, 1.0)


func _gui_input(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.pressed and event.button_index == MOUSE_BUTTON_LEFT:
		site_clicked.emit(site_at(event.position))
		accept_event()


## The site under a point in this control's coordinates, or -1 outside the grid.
func site_at(point: Vector2) -> int:
	if texture == null:
		return -1
	var cell := ((point - origin) / zoom).floor()
	var w := int(texture.get_width())
	var h := int(texture.get_height())
	if cell.x < 0 or cell.y < 0 or cell.x >= w or cell.y >= h:
		return -1
	return int(cell.y) * w + int(cell.x)
