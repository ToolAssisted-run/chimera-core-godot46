# Prints one line a frame ("probe f=..."), and one for every input event
# ("probe event ..."), to the log the core's tests read. Also draws them.
extends Node2D

var frame := 0
var rng := RandomNumberGenerator.new()
var saved := ""


func _ready() -> void:
	rng.randomize()
	# saves read at start and written again: user:// is the core's memory,
	# and a project's save data is put there before the game starts
	var f := FileAccess.open("user://probe.txt", FileAccess.READ)
	if f:
		saved = f.get_as_text()
	var slot := ""
	f = FileAccess.open("user://saves/slot.dat", FileAccess.READ)
	if f:
		slot = f.get_as_text()
	f = FileAccess.open("user://probe.txt", FileAccess.WRITE)
	f.store_string("written at unix %d" % int(Time.get_unix_time_from_system()))
	DirAccess.make_dir_recursive_absolute("user://saves")
	f = FileAccess.open("user://saves/slot.dat", FileAccess.WRITE)
	f.store_string("slot of frame %d" % Engine.get_process_frames())
	f = null
	print("probe renderer=[%s] [%s]" % [RenderingServer.get_video_adapter_name(), RenderingServer.get_video_adapter_api_version()])
	print("probe ready unix=%d date=%s ticks=%d rnd=%d randf=%.6f window=%s os=%s saved=[%s] slot=[%s]" % [
		int(Time.get_unix_time_from_system()),
		Time.get_datetime_string_from_system(true),
		Time.get_ticks_msec(),
		rng.randi(),
		randf(),
		str(DisplayServer.window_get_size()),
		OS.get_name(),
		saved,
		slot])


func _input(event: InputEvent) -> void:
	if event is InputEventKey:
		print("probe event f=%d key %s %s unicode=%d shift=%s" % [frame, OS.get_keycode_string(event.keycode), "down" if event.pressed else "up", event.unicode, str(event.shift_pressed)])
	elif event is InputEventMouseButton:
		print("probe event f=%d mouse button %d %s at %s double=%s" % [frame, event.button_index, "down" if event.pressed else "up", str(event.position), str(event.double_click)])
	elif event is InputEventMouseMotion:
		print("probe event f=%d mouse moved to %s by %s" % [frame, str(event.position), str(event.relative)])
	elif event is InputEventJoypadButton:
		print("probe event f=%d pad button %d %s" % [frame, event.button_index, "down" if event.pressed else "up"])
	elif event is InputEventJoypadMotion:
		print("probe event f=%d pad axis %d %.4f" % [frame, event.axis, event.axis_value])


func _process(delta: float) -> void:
	frame += 1
	var line := "probe f=%d ticks=%d delta=%.6f unix=%.3f rnd=%d mouse=%s lx=%.4f rt=%.4f" % [
		frame,
		Time.get_ticks_usec(),
		delta,
		Time.get_unix_time_from_system(),
		rng.randi() % 1000,
		str(get_viewport().get_mouse_position()),
		Input.get_joy_axis(0, JOY_AXIS_LEFT_X),
		Input.get_joy_axis(0, JOY_AXIS_TRIGGER_RIGHT)]
	print(line)
	$Label.text = line.replace(" ", "\n")
	queue_redraw()


func _draw() -> void:
	# something that moves, so pictures of two frames differ
	draw_rect(Rect2(Vector2((frame * 3) % 300, 200), Vector2(16, 16)), Color(1, 0.5, 0.2))
