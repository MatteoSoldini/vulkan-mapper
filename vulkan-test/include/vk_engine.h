#pragma once

#define NOMINMAX    // To solve the problem caused by the <Windows.h> header file that includes macro definitions named max and min

#define VK_USE_PLATFORM_WIN32_KHR // Windows specific window system details
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <vector>
#include <optional>
#include "../include/scene.h"


struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class VulkanEngine {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }
    

private:
    GLFWwindow* window;
    VkInstance instance;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;
    VkQueue graphicsQueue;
    VkSurfaceKHR surface;
    VkQueue presentQueue;
    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;
    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;
    std::vector<VkFramebuffer> swapChainFramebuffers;
    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> descriptorSets;

    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;

    bool framebufferResized = false;
    Scene scene;

    // Syncronization
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    uint32_t currentFrame = 0;

    const uint32_t WIDTH = 800;
    const uint32_t HEIGHT = 600;

    const int MAX_FRAMES_IN_FLIGHT = 2;

    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    #ifdef NDEBUG
        const bool enableValidationLayers = false;
    #else
        const bool enableValidationLayers = true;
    #endif

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);

    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    bool checkValidationLayerSupport();

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

    static void cursor_position_callback(GLFWwindow * window, double xpos, double ypos);
    
    static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);

    void initWindow();

    void createInstance();

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

    int rateDeviceSuitability(VkPhysicalDevice device);

    bool checkDeviceExtensionSupport(VkPhysicalDevice device);

    void createSurface();

    void pickPhysicalDevice();

    void createLogicalDevice();

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    void createSwapChain();

    void createImageViews();

    VkShaderModule createShaderModule(const std::vector<char>& code);

    void createRenderPass();

    void createGraphicsPipeline();

    void createFramebuffers();

    void createCommandPool();

    void createCommandBuffers();

    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    void createSyncObjects();

    void recreateSwapChain();

    void createVertexBuffer();

    void createIndexBuffer();

    void createDescriptorSetLayout();

    void createUniformBuffers();

    void createDescriptorPool();

    void createDescriptorSets();

    void initVulkan();

    void updateUniformBuffer(uint32_t currentImage);

    void updateVertexBuffer();

    void updateIndexBuffer();

    void drawFrame();

    void mainLoop();

    void cleanupSwapChain();

    void cleanup();
};