#pragma once

#define NOMINMAX    // To solve the problem caused by the <Windows.h> header file that includes macro definitions named max and min

#define VK_USE_PLATFORM_WIN32_KHR // Windows specific window system details
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

class VulkanOutput {
public:
	VulkanOutput(VkInstance instance, VkPhysicalDevice physicalDevice);

	void run() {
		initSurface();
	}

private:
	const uint32_t WIDTH = 1280;
	const uint32_t HEIGHT = 720;

	// vulkan
	VkInstance instance;
	VkPhysicalDevice physicalDevice;

	// surface
	GLFWwindow* window;
	VkSurfaceKHR surface;

	// init surface
	void initSurface();
};