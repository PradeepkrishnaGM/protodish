extends VBoxContainer
## Color legend of the current view mode, built from ProtodishWorld.get_legend(): a swatch
## per single color, a gradient bar with its two end labels per color run.

const SWATCH := Vector2(14, 14)
const SWATCH_BORDER := Color(0.45, 0.47, 0.5)
const BAR_HEIGHT := 10


func show_items(items: Array) -> void:
	for child in get_children():
		remove_child(child)
		child.queue_free()
	for item: Dictionary in items:
		var colors: PackedColorArray = item["colors"]
		if colors.size() == 1:
			add_child(_swatch_row(colors[0], item["label"]))
		else:
			add_child(_gradient(colors, item["label"], item["low"], item["high"]))


func _swatch_row(color: Color, text: String) -> Control:
	var row := HBoxContainer.new()
	var swatch := Panel.new()
	var style := StyleBoxFlat.new()
	style.bg_color = color
	style.set_border_width_all(1)  # keeps dark swatches visible on the dark panel
	style.border_color = SWATCH_BORDER
	swatch.add_theme_stylebox_override("panel", style)
	swatch.custom_minimum_size = SWATCH
	swatch.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	row.add_child(swatch)
	row.add_child(_label(text))
	return row


func _gradient(colors: PackedColorArray, text: String, low: String, high: String) -> Control:
	var box := VBoxContainer.new()
	box.add_theme_constant_override("separation", 1)
	box.add_child(_label(text))
	var gradient := Gradient.new()
	var offsets := PackedFloat32Array()
	for k in colors.size():
		offsets.append(float(k) / (colors.size() - 1))
	gradient.offsets = offsets
	gradient.colors = colors
	var tex := GradientTexture1D.new()
	tex.gradient = gradient
	var bar := TextureRect.new()
	bar.texture = tex
	bar.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	bar.stretch_mode = TextureRect.STRETCH_SCALE
	bar.custom_minimum_size = Vector2(0, BAR_HEIGHT)
	box.add_child(bar)
	var ends := HBoxContainer.new()
	ends.add_child(_label(low, true))
	var spacer := Control.new()
	spacer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	ends.add_child(spacer)
	ends.add_child(_label(high, true))
	box.add_child(ends)
	return box


func _label(text: String, small := false) -> Label:
	var label := Label.new()
	label.text = text
	label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	label.add_theme_font_size_override("font_size", 12 if small else 14)
	if small:
		label.size_flags_horizontal = Control.SIZE_FILL
		label.add_theme_color_override("font_color", Color(0.7, 0.72, 0.75))
	return label
