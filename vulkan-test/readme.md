# to do

# future
- isolate imgui init function in its own class
- video input implementation (ex. OBS can be used as a source)
	- OBS can be used to implement scenes & sound
- plane move could be more efficient, now is computing homography matrix on single marker move, when moving a plane all 4 markers gets moved one at a time

# done
- [x] draw imgui
	- following [this guide](https://frguthmann.github.io/posts/vulkan_imgui/)
- [x] add the main scene as a vulkan viewport inside ImGui
	- [x] refactor to present only imgui
	- [x] render the scene into an image view using framebuffers (offscreen rendering)
		- [x] To check: as many multiple frames in flight
			- the command buffer follows the same principle (may not be correct for imageviews/framebuffers?)
		- useful links:
			- [Github repo](https://github.com/SaschaWillems/Vulkan/blob/master/examples/offscreen/offscreen.cpp)
	- [x] use the result in imgui as a texture
		- Useful links:
			- [Github issue](https://github.com/ocornut/imgui/issues/5110)
	- [x] pass correct mouse position to scene
		- rendering viewport during imgui rendering but before adding image
	- [x] cleanup
- [x] draw line around planes
	- [x] CHECKME: vertex/index buffer passed to pipeline