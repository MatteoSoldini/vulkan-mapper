add_executable(VulkanMapper 
	main.cpp
	include/vk_types.h
	"include/vk_state.h"
	"src/vk_state.cpp"
	include/vk_output.h
	src/vk_output.cpp
	include/scene.h
	src/scene.cpp
	include/media_manager.h
	src/media_manager.cpp
	include/read_file.h
	src/read_file.cpp
	include/ui.h
	src/ui.cpp
	"include/scene_objects.h"
	"src/scene_objects.cpp"
	include/vk_utils.h
	src/vk_utils.cpp
	include/vk_video.h
	src/vk_video.cpp
	include/video.h
	src/video.cpp
 "include/vm_types.h" "include/media.h" "include/image.h" "src/image.cpp"   "include/app.h" "src/app.cpp")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET VulkanMapper PROPERTY CXX_STANDARD 20)
endif()

# shaders
file(GLOB_RECURSE SPIRV_FILES
    "${PROJECT_SOURCE_DIR}/VulkanMapper/shaders/*.spv"
)

foreach(SPV ${SPIRV_FILES})
	message(STATUS "BUILDING SHADER")
	get_filename_component(SPV_NAME ${SPV} NAME)
	
	message(STATUS ${SPV_NAME})

	add_custom_target(${SPV_NAME} ALL
		COMMAND ${CMAKE_COMMAND} -E copy
		${SPV}
		${PROJECT_BINARY_DIR}/VulkanMapper/shaders/${SPV_NAME}
		COMMENT "Copying resources into binary directory"
	)

	add_dependencies(VulkanMapper ${SPV_NAME})
endforeach(SPV)

# vulkan
find_package(Vulkan REQUIRED)

# volk
add_subdirectory(dependencies/volk)
target_link_libraries(VulkanMapper PRIVATE volk_headers)
target_compile_definitions(VulkanMapper PRIVATE VK_NO_PROTOTYPES)

# glfw
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(dependencies/glfw)
target_link_libraries(VulkanMapper PRIVATE glfw)

# imgui
add_library(imgui STATIC)

target_sources(imgui
    PRIVATE dependencies/imgui/imgui.cpp
	PRIVATE dependencies/imgui/imgui_demo.cpp
    PRIVATE dependencies/imgui/imgui_draw.cpp
    PRIVATE dependencies/imgui/imgui_tables.cpp
    PRIVATE dependencies/imgui/imgui_widgets.cpp

	PRIVATE dependencies/imgui/backends/imgui_impl_vulkan.cpp
	PRIVATE dependencies/imgui/backends/imgui_impl_glfw.cpp
)

target_include_directories(imgui
	PUBLIC dependencies/imgui/
    
	PUBLIC dependencies/imgui/backends
)
target_link_libraries(imgui PRIVATE glfw)
target_compile_definitions(imgui PRIVATE VK_NO_PROTOTYPES)
target_link_libraries(imgui PRIVATE Vulkan::Vulkan)

target_link_libraries(VulkanMapper PRIVATE imgui)

#add_library(glm INTERFACE)
#target_include_directories(glm
#	INTERFACE dependencies/glm
#)

# glm
add_subdirectory(dependencies/glm)
target_link_libraries(VulkanMapper PRIVATE glm)

# minimp4
add_library(minimp4 INTERFACE)
target_include_directories(minimp4
	INTERFACE dependencies/minimp4
)
target_link_libraries(VulkanMapper PRIVATE minimp4)

# stb
add_library(stb INTERFACE)
target_include_directories(stb
	INTERFACE dependencies/stb
)
target_link_libraries(VulkanMapper PRIVATE stb)

# h264
add_library(h264 INTERFACE)
target_include_directories(h264
	INTERFACE dependencies/h264
)
target_link_libraries(VulkanMapper PRIVATE h264)

# nfd
add_library(nfd STATIC)
target_include_directories(nfd
	PUBLIC dependencies/nfd/src/include
)
target_sources(nfd
    PRIVATE dependencies/nfd/src/nfd_common.c
)
if (WIN32)
	target_sources(nfd
		PRIVATE dependencies/nfd/src/nfd_win.cpp
	)
endif()


target_link_libraries(VulkanMapper PRIVATE nfd)