extends Node3D

func _ready():
	var s = GDSoot.new()
	if not s.initialize(""):
		print("init failed")
		return

	if not s.load_module("test.bin"):
		print("load failed")
		return

	var result = s.run_top_level("test")
	print("top-level: ", result)

	var v = s.call("my-function", [1, 2, 3])
	print("call result: ", v)

	s.shutdown()
