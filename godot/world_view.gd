extends Control
## Draws the world image at the largest whole-number zoom that fits, centered, so every
## site is the same number of pixels. site_at() maps a point back to a site.

var texture: Texture2D:
	set(value):
		texture = value
		queue_redraw()

var zoom := 1  ## screen pixels per site
var origin := Vector2.ZERO  ## top-left corner of the image inside this control


func _ready() -> void:
	resized.connect(queue_redraw)


func _draw() -> void:
	if texture == null:
		return
	var size_px := texture.get_size()
	zoom = maxi(1, floori(minf(size.x / size_px.x, size.y / size_px.y)))
	var shown := size_px * zoom
	origin = ((size - shown) / 2).floor()
	draw_texture_rect(texture, Rect2(origin, shown), false)


## The site under a point in this control's coordinates, or -1 outside the image.
func site_at(point: Vector2) -> int:
	if texture == null:
		return -1
	var cell := ((point - origin) / zoom).floor()
	var w := int(texture.get_width())
	var h := int(texture.get_height())
	if cell.x < 0 or cell.y < 0 or cell.x >= w or cell.y >= h:
		return -1
	return int(cell.y) * w + int(cell.x)
