# to do
- [ ] add the main scene as a vulkan viewport inside ImGui
	- [x] refactor to present only imgui
	- [x] render the scene into an image view using framebuffers (offscreen rendering)
		- multiple images in flight
		- useful links:
			- [Github repo](https://github.com/SaschaWillems/Vulkan/blob/master/examples/offscreen/offscreen.cpp)
	- [x] use the result in imgui as a texture
		- Useful links:
			- [Github issue](https://github.com/ocornut/imgui/issues/5110)
	- [ ] pass correct mouse position to scene
		- rendering viewport during imgui rendering but before adding image

# future
- isolate imgui init function in its own class
- video input implementation (ex. OBS can be used as a source)
	- OBS can be used to implement scenes & sound

# done
- [x] draw imgui
	- following [this guide](https://frguthmann.github.io/posts/vulkan_imgui/)
