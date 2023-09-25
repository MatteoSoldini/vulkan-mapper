# doing
- [ ] fullscreen projection window
	- [x] implementation
	- [ ] test on second monitor
	- [ ] FIXME: closing app with projection window open causes validation error
- [ ] Video
	- Useful links:
		1. [wicked engine blog](https://wickedengine.net/2023/05/07/vulkan-video-decoding/)
		2. [wicked engine github]()
	- Dynamic load extension functions
		- [reddit comment](https://www.reddit.com/r/vulkan/comments/jeolie/linker_error_using/)
		- (alternative) [volk](https://github.com/zeux/volk)
		- using volk
	- [ ] load video
	- [x] init decode
	- [x] create textures
	- [ ] decode frame

# to do
- isolate imgui init function in its own class
- video input implementation (ex. OBS can be used as a source)
	- OBS can be used to implement scenes & sound
- plane move could be more efficient, now is computing homography matrix on single marker move, when moving a plane all 4 markers gets moved one at a time
- video playback
- scenes
- timelines
- input area for each plane
- viewport panning/zooming
- draw screen area to viewport
- draw markers as pixel size (relative to viewport size)


# done
- [x] draw imgui
	- following [this guide](https://frguthmann.github.io/posts/vulkan_imgui/)
- [x] add the main scene as a vulkan viewport inside ImGui
	- [x] refactor to present only imgui
	- [x] render the scene into an image view using framebuffers (offscreen rendering)
		- [x] To check: as many multiple frames in flight
			- the command buffer follows the same principle (may not be correct for imageviews/framebuffers?)
		- useful links:
			- [github repo](https://github.com/SaschaWillems/Vulkan/blob/master/examples/offscreen/offscreen.cpp)
	- [x] use the result in imgui as a texture
		- Useful links:
			- [github issue](https://github.com/ocornut/imgui/issues/5110)
	- [x] pass correct mouse position to scene
		- rendering viewport during imgui rendering but before adding image
	- [x] cleanup
- [x] draw line around planes
	- [x] CHECKME: vertex/index buffer passed to pipeline
- [x] media manager
	- plane-to-media binding
		- multiple media could be used by multiple planes in multiple scenes
	- [x] load files (images)
	- [x] show files in list
	- [x] with a plane selected, bind a image to the plane
		- [x] highlight binded image