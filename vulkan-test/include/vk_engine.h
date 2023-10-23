#pragma once

#define NOMINMAX    // To solve the problem caused by the <Windows.h> header file that includes macro definitions named max and min

#include <volk.h>
//#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <vector>
#include <optional>
#include <string>
#include <map>
#include "scene.h"
#include "ui.h"
#include "media_manager.h"
#include "vk_output.h"
#include "vk_utils.h"

#include <imgui.h>

struct PipelineToLoad {
    std::string name;
    std::string vertexShaderFile;
    std::string fragmentShaderFile;
    VkPrimitiveTopology drawTopology;
    std::vector<VkDescriptorSetLayout*> descriptorSetLayouts;
};

// runtime object
struct Texture{
    VkImage image;
    VkDeviceMemory imageMemory;
    VkImageView imageView;
    VkDescriptorSet descriptorSet;
    uint32_t width;
    uint32_t height;
};

// compile time object
struct Pipeline {
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
};

class Scene;

class UI;

class MediaManager;

class VulkanOutput;

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
        PipelineToLoad{"color", "shaders/vert.spv", "shaders/col.spv", VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, {&uniformBufferLayout}},
        PipelineToLoad{"texture", "shaders/vert.spv", "shaders/text.spv", VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, {&uniformBufferLayout, &textureLayout}},
        PipelineToLoad{"line", "shaders/vert.spv", "shaders/col.spv", VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, {&uniformBufferLayout} },
        PipelineToLoad{"video_frame", "shaders/vert.spv", "shaders/text.spv", VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, {&uniformBufferLayout, &videoFrameLayout}},
    };

    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

    const std::vector<const char*> deviceExtensions = {
        // presentation
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        // video decode
        
        VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME,
        VK_KHR_VIDEO_QUEUE_EXTENSION_NAME,
        VK_KHR_VIDEO_DECODE_QUEUE_EXTENSION_NAME,
        VK_KHR_VIDEO_DECODE_H264_EXTENSION_NAME,
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
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> uniformBufferSets;
    
    // samplers
    VkSampler textureSampler;
    VkSamplerYcbcrConversion ycbcrSamplerConversion;
    VkSamplerYcbcrConversionInfo ycbcrSamplerConversionInfo;
    VkSampler ycbcrFrameSampler;

    // command pools - graphics
    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;

    // command pools - video decode
    VkQueue videoQueue;
    VkCommandPool videoCommandPool;
    VkCommandBuffer videoCommandBuffer;

    // queue families
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    std::optional<uint32_t> videoFamily;

    // viewport rendering
    VkRenderPass viewportRenderpass;
    std::vector<Texture> viewportTextures;
    std::vector<VkFramebuffer> viewportFramebuffers;

    // textures
    std::map<uint8_t, Texture> textures;

    // pipelines
    std::map<std::string, Pipeline> pipelines;

    // descriptor set layouts
    VkDescriptorSetLayout textureLayout;
    VkDescriptorSetLayout videoFrameLayout;
    VkDescriptorSetLayout uniformBufferLayout;

    // uniform buffer
    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;

    bool framebufferResized = false;

    // scene
    Scene* pScene;

    // media manager
    MediaManager* pMediaManager;

    // output
    VulkanOutput* pOutput;
    bool showOutput = false;

    // imgui
    VkCommandPool imGuiCommandPool;
    std::vector<VkCommandBuffer> imGuiCommandBuffers;
    VkRenderPass imGuiRenderPass;
    std::vector<VkFramebuffer> imGuiFrameBuffers;
    VkDescriptorPool imguiDescriptorPool;

    UI* pUi;

    // syncronization
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    uint32_t currentFrame = 0;
    
    // validation layer
    #ifdef NDEBUG
        const bool enableValidationLayers = false;
    #else
        const bool enableValidationLayers = true;
    #endif
    bool checkValidationLayerSupport();

    // command buffer
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

    static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);

    void initWindow();

    void createInstance();
    
    int rateDeviceSuitability(VkPhysicalDevice device);

    bool checkDeviceExtensionSupport(VkPhysicalDevice device);

    void createSurface();

    void pickPhysicalDevice();

    void createLogicalDevice();

    void queryQueueFamilies();

    void createSwapChain();

    void createImageViews();

    void createRenderPass();

    void loadPipelines();

    void createFramebuffers(std::vector<VkFramebuffer>& frameBuffers, VkRenderPass renderPass);

    void createCommandPools();

    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    void createSyncObjects();

    void recreateSwapChain();

    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

    void createSamplers();

    void createVertexBuffer();

    void createIndexBuffer();

    void createDescriptorSetLayouts();

    void createUniformBuffers();

    void createDescriptorPool();

    void createStaticDescriptorSets();

    void initVulkan();

    void updateUniformBuffer(uint32_t currentImage);

    void updateVertexBuffer();

    void updateIndexBuffer();

    void drawFrame();

    void mainLoop();

    void cleanupSwapChain();

    void cleanup();

    // imgui
    void initImGui();
    void imGuiCleanup();
    void recordImGuiCommandBuffer(uint32_t imageIndex);

    // viewport
    void initViewportRender();
    void cleanupViewportRender();
    void recreateViewportSurface(uint32_t width, uint32_t height, uint32_t surfaceIndex);

    // TEMP
    VkDescriptorSet videoFrameView;

public:
    void loadTexture(uint8_t id, unsigned char* pixels, int width, int height);

    void loadVideoFrame(VkImageView frameView);

    // output
    bool getShowOutput();
    void setShowOutput(bool showOutput);

    // viewport
    VkDescriptorSet renderViewport(uint32_t viewportWidth, uint32_t viewportHeight, uint32_t cursorPosX, uint32_t cursorPosY);
    
    // scene
    Scene* getScene() { return pScene; }

    // media manager
    MediaManager* getMediaManager() { return pMediaManager; }
    
    // device
    VkDevice getDevice() { return device; }

    // physicalDevice
    VkPhysicalDevice getPhysicalDevice() { return physicalDevice; }

    // command buffers
    std::vector<VkCommandBuffer> getCommandBuffers() { return commandBuffers; }
    VkCommandBuffer getVideoCommandBuffer() { return videoCommandBuffer; }

    // queues
    VkQueue getVideoQueue() { return videoQueue; }
    
    // queue indexes
    uint32_t getVideoQueueFamilyIndex() { return videoFamily.value(); }

    // image operations
    void createImage(uint32_t width, uint32_t height, uint32_t arrayLayers, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory, const void* pNext);
    void transitionImageLayout(VkImage image, VkFormat format, uint32_t layerCount, VkImageLayout oldLayout, VkImageLayout newLayout);
    VkImageView createImageView(VkImage image, VkFormat format, void* pNext);

    // samplers
    VkSampler getYcbcrFrameSampler() { return ycbcrFrameSampler; }
    VkSamplerYcbcrConversion getYcbcrSamplerConversion() { return ycbcrSamplerConversion; }

    // buffers operations
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory, void* pNext);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
};