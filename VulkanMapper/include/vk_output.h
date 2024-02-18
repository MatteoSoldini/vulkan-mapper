#pragma once
#define NOMINMAX    // To solve the problem caused by the <Windows.h> header file that includes macro definitions named max and min

#include <volk.h>

//#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <vector>
#include "app.h"

struct Pipeline;

struct VmTexture;

struct Scene;

class VulkanState;

class App;


class VulkanOutput {
public:
	VulkanOutput(App* pApp);

	void init(int monitorNum);

	void draw();

	void cleanup();

	int getFramesInFlight() { return MAX_FRAMES_IN_FLIGHT; };


private:
	const int MAX_FRAMES_IN_FLIGHT = 2;

	App* pApp;

	// surface
	GLFWwindow* window;
	VkSurfaceKHR surface;
	VkSwapchainKHR swapChain;
	std::vector<VkImage> swapChainImages;
	std::vector<VkImageView> swapChainImageViews;
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;

	// syncronization
	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<VkFence> inFlightFences;
	uint32_t currentFrame = 0;

	// command pool
	VkCommandPool commandPool;
	std::vector<VkCommandBuffer> commandBuffers;

	// rendering
	VkRenderPass renderPass;
	std::map<std::string, Pipeline> pipelines;
	std::vector<VkFramebuffer> frameBuffers;

	// uniform buffer for output mvp matrix
	std::vector<VkBuffer> uniformBuffers;
	std::vector<VkDeviceMemory> uniformBuffersMemory;
	std::vector<void*> uniformBuffersMapped;
	VkDescriptorPool descriptorPool;
	std::vector<VkDescriptorSet> uniformBufferSets;

	static void keyboardCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

	void initWindow(GLFWmonitor* monitor);
	void initSurface();
	void initSyncObjects();
	void initCommandPool();
	void initRenderPass();
	void initFrameBuffer();
	void loadPipelines();
	void initMemory();
	
	void recordCommandBuffer(uint32_t imageIndex);

	void recreateSwapChain();
};