extends Control
## Population over the whole run: cells, producers and infected cells, one point per
## pixel column, each the peak of the ticks it covers. Faint lines mark the years.

const SERIES := [
	["cells", Color(0.92, 0.93, 0.95)],
	["producers", Color8(90, 200, 80)],
	["infected", Color8(225, 80, 200)],
]
const GRID := Color(1, 1, 1, 0.08)
const AXIS_TEXT := Color(0.6, 0.62, 0.65)
const PAD := Vector2(4, 4)

var history := {}
var year_length := 2000
var last_tick := 0


func set_data(new_history: Dictionary, new_year_length: int, tick: int) -> void:
	history = new_history
	year_length = new_year_length
	last_tick = tick
	queue_redraw()


## Points the graph asks for: one per pixel column.
func max_points() -> int:
	return maxi(1, int(size.x - 2 * PAD.x))


func _draw() -> void:
	draw_rect(Rect2(Vector2.ZERO, size), Color(0, 0, 0, 0.25))
	if history.is_empty() or (history["ticks"] as PackedInt64Array).is_empty():
		return
	var ticks: PackedInt64Array = history["ticks"]
	var top := 1
	for s in SERIES:
		for v in (history[s[0]] as PackedInt32Array):
			top = maxi(top, v)
	top = _nice_ceiling(top)
	var area := Rect2(PAD, size - 2 * PAD)
	var span := maxf(1.0, float(last_tick))

	# Year lines, thinned out so they stay at least 6 px apart.
	var every := 1
	while every * year_length / span * area.size.x < 6.0:
		every *= 2
	var y := every
	while y * year_length <= last_tick:
		var x := area.position.x + y * year_length / span * area.size.x
		draw_line(Vector2(x, area.position.y), Vector2(x, area.end.y), GRID)
		y += every

	for s in SERIES:
		var values: PackedInt32Array = history[s[0]]
		var points := PackedVector2Array()
		for k in ticks.size():
			points.append(Vector2(
				area.position.x + ticks[k] / span * area.size.x,
				area.end.y - float(values[k]) / top * area.size.y))
		if points.size() == 1:
			points.append(points[0] + Vector2(1, 0))
		draw_polyline(points, s[1], 1.0, true)

	var font := get_theme_default_font()
	draw_string(font, PAD + Vector2(2, 12), _thousands(top), HORIZONTAL_ALIGNMENT_LEFT, -1, 11, AXIS_TEXT)
	var span_text := "ticks 0 – %s, lines = years" % _thousands(last_tick)
	draw_string(font, Vector2(PAD.x, PAD.y + 12), span_text, HORIZONTAL_ALIGNMENT_RIGHT, area.size.x - 2, 11, AXIS_TEXT)


## 1, 2 or 5 times a power of ten, at least n.
static func _nice_ceiling(n: int) -> int:
	var step := 1
	while true:
		for m in [1, 2, 5]:
			if m * step >= n:
				return m * step
		step *= 10
	return n


static func _thousands(n: int) -> String:
	var s := str(n)
	var out := ""
	while s.length() > 3:
		out = "," + s.right(3) + out
		s = s.left(s.length() - 3)
	return s + out
