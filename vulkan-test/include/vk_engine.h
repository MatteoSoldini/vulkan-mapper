#pragma once

#define NOMINMAX    // To solve the problem caused by the <Windows.h> header file that includes macro definitions named max and min

#define VK_USE_PLATFORM_WIN32_KHR // Windows specific window system details
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <vector>
#include <optional>
#include <string>
#include <map>
#include "scene.h"
#include "ui.h"

#include <imgui.h>


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

struct PipelineToLoad {
    std::string name;
    std::string vertex_shader_file;
    std::string fragment_shader_file;
};

// runtime object
struct Texture {
    VkImage image;
    VkDeviceMemory image_memory;
    VkImageView image_view;
    VkDescriptorSet descriptor_set;
};

// compile time object
struct Pipeline {
    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;
};

class Scene;

class UI;

class VulkanEngine {
public:
    VulkanEngine();

    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }
    
private:
    // constants
    const uint32_t WIDTH = 1280;
    const uint32_t HEIGHT = 720;

    const int MAX_FRAMES_IN_FLIGHT = 2;

    const std::vector<PipelineToLoad> pipelinesToLoad = {
        PipelineToLoad{"color", "shaders/vert.spv", "shaders/col.spv"},
        PipelineToLoad{"texture", "shaders/vert.spv", "shaders/text.spv"},
    };

    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
    
    const uint32_t VERTICES_COUNT = 128;
    const uint32_t INDICES_COUNT = 128;

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
    //VkRenderPass renderPass;
    //std::vector<VkFramebuffer> swapChainFramebuffers;
    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> uniform_buffer_sets;
    VkSampler textureSampler;

    // offscreen rendering
    VkRenderPass offscreenRenderpass;
    std::vector<Texture> offscreenTextures;
    std::vector<VkFramebuffer> offscreenFramebuffers;
    uint32_t offscreenViewportWidth = 0;
    uint32_t offscreenViewportHeight = 0;

    // textures
    std::map<std::string, Texture> textures;

    // pipelines
    std::map<std::string, Pipeline> pipelines;

    // descriptor set layouts
    VkDescriptorSetLayout singleTextureLayout;
    VkDescriptorSetLayout uniformBufferLayout;

    // uniform buffer
    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;

    bool framebufferResized = false;

    // scene
    Scene* pScene;

    // imgui
    VkCommandPool imGuiCommandPool;
    std::vector<VkCommandBuffer> imGuiCommandBuffers;
    VkRenderPass imGuiRenderPass;
    std::vector<VkFramebuffer> imGuiFrameBuffers;
    VkDescriptorPool imguiDescriptorPool;

    UI* pUi;

    // Syncronization
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    uint32_t currentFrame = 0;

    #ifdef NDEBUG
        const bool enableValidationLayers = false;
    #else
        const bool enableValidationLayers = true;
    #endif

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);

    VkCommandBuffer beginSingleTimeCommands();

    void endSingleTimeCommands(VkCommandBuffer commandBuffer);

    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    VkImageView createImageView(VkImage image, VkFormat format);

    bool checkValidationLayerSupport();

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

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

    void initOffscreenRender();

    VkShaderModule createShaderModule(const std::vector<char>& code);

    void createOffscreenRenderPass();

    void createRenderPass();

    void loadPipelines();

    void createFramebuffers(std::vector<VkFramebuffer>& frameBuffers, VkRenderPass renderPass);

    void createCommandPool(VkCommandPool* commandPool, VkCommandPoolCreateFlags flags);

    void createCommandBuffers(std::vector<VkCommandBuffer>& commandBuffers, VkCommandPool& commandPool);

    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    void createSyncObjects();

    void recreateSwapChain();

    void recreateOffscreenViewport(uint32_t width, uint32_t height);

    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);

    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

    void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);

    void createTextureSampler();

    void createVertexBuffer();

    void createIndexBuffer();

    void createDescriptorSetLayouts();

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

    void initImGui();

    void recordImGuiCommandBuffer(uint32_t imageIndex);

    void imGuiCleanup();

public:
    void loadTexture(std::string imagePath);

    void renderViewport(uint32_t viewportWidth, uint32_t viewportHeight, uint32_t cursorPosX, uint32_t cursorPosY);
};