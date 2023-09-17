#pragma once

#define NOMINMAX    // To solve the problem caused by the <Windows.h> header file that includes macro definitions named max and min

#define VK_USE_PLATFORM_WIN32_KHR // Windows specific window system details
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <vector>
#include "scene.h"
#include "vk_engine.h"

struct Pipeline;

struct Texture;

struct OutputSharedEngineState {
	VkInstance instance;
	VkDevice device;
	uint32_t graphicsFamily;
	uint32_t presentFamily;
	VkQueue graphicsQueue;
	VkQueue presentQueue;
	VkPhysicalDevice physicalDevice;
	VkBuffer vertexBuffer;
	VkBuffer indexBuffer;
	std::map<uint8_t, Texture>* pTextures;
	VkDescriptorSetLayout uniformBufferLayout;
	VkDescriptorSetLayout singleTextureLayout;
	Scene* pScene;
};

class VulkanOutput {
public:
	VulkanOutput(OutputSharedEngineState sharedEngineState);

	void init() {
		initWindow();
		initSurface();
		initSyncObjects();
		initCommandPool();
		initRenderPass();
		initFrameBuffer();
		loadPipeline();
		initUniformBuffer();
	}

	void drawFrame();

	void close();


private:
	const uint32_t WIDTH = 1280;
	const uint32_t HEIGHT = 720;

	const int MAX_FRAMES_IN_FLIGHT = 2;

	OutputSharedEngineState sharedEngineState;

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
	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;
	std::vector<VkFramebuffer> frameBuffers;

	// uniform buffer
	std::vector<VkBuffer> uniformBuffers;
	std::vector<VkDeviceMemory> uniformBuffersMemory;
	std::vector<void*> uniformBuffersMapped;
	VkDescriptorPool descriptorPool;
	std::vector<VkDescriptorSet> uniformBufferSets;

	void initWindow();
	void initSurface();
	void initSyncObjects();
	void initCommandPool();
	void initRenderPass();
	void initFrameBuffer();
	void loadPipeline();
	void initUniformBuffer();
	
	void recordCommandBuffer(uint32_t imageIndex);

	void recreateSwapChain();
};