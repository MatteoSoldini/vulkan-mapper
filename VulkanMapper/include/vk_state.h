#pragma once

#define NOMINMAX    // To solve the problem caused by the <Windows.h> header file that includes macro definitions named max and min

#include <volk.h>

#define GLFW_INCLUDE_NONE
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
#include "vm_types.h"
#include "app.h"

struct PipelineToLoad {
    std::string name;
    std::string vertexShaderFile;
    std::string fragmentShaderFile;
    VkPrimitiveTopology drawTopology;
    std::vector<VkDescriptorSetLayout*> descriptorSetLayouts;
};

typedef uint8_t VmTextureId_t;

struct VmTexture{
    VmTextureId_t id;
    VkImage image;
    VkDeviceMemory imageMemory;
    VkImageView imageView;
    VkDescriptorSet descriptorSet;
    uint32_t width;
    uint32_t height;
};

struct Pipeline {
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
};

// video frames sync elements for rendering
// should be created for every video stream
struct VmVideoFrameStream {
    VmVideoFrameStreamId_t id;
    VkImageView frameImageView = VK_NULL_HANDLE;            // the current video stream frame

    // viewport
    std::vector<VkDescriptorSet> vpDescriptorSetsInFlight;  // the viewport's descriptor sets in flight
    std::vector<VkImageView> vpImageViewsInFlight;          // the current image view corrisponding to the descriptor set

    // output
    std::vector<VkDescriptorSet> outDescriptorSetsInFlight; // the output's descriptor sets in flight
    std::vector<VkImageView> outImageViewsInFlight;         // the current image view corrisponding to the descriptor set
};

class Scene;

class UI;

class MediaManager;

class VulkanOutput;

class App;

class VulkanState {
private:
    App* pApp;

public:
    VulkanState(App* pApp);

    void init();

    void draw();

    void cleanup();
    
private:
    UI* pUi;
    
    // constants
    const uint32_t WIDTH = 1920;
    const uint32_t HEIGHT = 1080;

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

    // queue families
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    std::optional<uint32_t> videoFamily;

    // viewport rendering
    VkRenderPass viewportRenderpass;
    std::vector<VmTexture> viewportTextures;
    std::vector<VkFramebuffer> viewportFramebuffers;

    // textures
    std::vector<VmTexture> textures;

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

    // imgui
    VkCommandPool imGuiCommandPool;
    std::vector<VkCommandBuffer> imGuiCommandBuffers;
    VkRenderPass imGuiRenderPass;
    std::vector<VkFramebuffer> imGuiFrameBuffers;
    VkDescriptorPool imguiDescriptorPool;

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

    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);

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

    void renderViewportFrame(VkCommandBuffer commandBuffer, uint32_t imageIndex);

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

    void cleanupSwapChain();

    // video frame streams
    std::vector<VmVideoFrameStream> vmVideoFrameStreams;
    
    //VkImageView prevVideoFrameView = VK_NULL_HANDLE;

    // viewport
    void initViewportRender();
    void cleanupViewportRender();
    void recreateViewportSurface(uint32_t width, uint32_t height, uint32_t surfaceIndex);

    // non-vk objects sections
    // TODO: move away from this class
    
    // imgui
    void initImGui();
    void imGuiCleanup();
    void recordImGuiCommandBuffer(uint32_t imageIndex);

public:
    // ----- viewport renderer -----
    VkDescriptorSet renderViewport(uint32_t viewportWidth, uint32_t viewportHeight, uint32_t cursorPosX, uint32_t cursorPosY);

    // ----- vulkan state -----
    // device
    VkDevice getDevice() { return device; }
    
    // instance
    VkInstance getInstance() { return instance; };
    
    // buffers
    VkBuffer getVertexBuffer() { return vertexBuffer; };
    VkBuffer getIndexBuffer() { return indexBuffer; };

    // textures
    VmTexture* getTexture(VmTextureId_t textureId);

    // descriptor sets layout
    VkDescriptorSetLayout getUniformBufferLayout() { return uniformBufferLayout; };
    VkDescriptorSetLayout getVideoFrameLayout() { return videoFrameLayout; };

    // pipelines
    Pipeline getPipeline(std::string pipelineName);
    std::vector<PipelineToLoad> getPipelinesToLoad() { return pipelinesToLoad; };

    VmTextureId_t loadTexture(unsigned char* pixels, int width, int height);
    void destroyTexture(VmTextureId_t textureId);

    // video frame stream
    VmVideoFrameStreamId_t createVideoFrameStream();
    VmVideoFrameStream* getVideoFrameStream(VmVideoFrameStreamId_t streamId);
    void removeVideoFrameStream(VmVideoFrameStreamId_t streamId);
    void loadVideoFrame(VmVideoFrameStreamId_t vmVideoFrameStreamId, VkImageView videoFrameView);
    
    // physicalDevice
    VkPhysicalDevice getPhysicalDevice() { return physicalDevice; }

    // command buffers
    std::vector<VkCommandBuffer> getCommandBuffers() { return commandBuffers; }
    VkCommandPool getVideoCommandPool() { return videoCommandPool; }

    // queues
    VkQueue getGraphicsQueue() { return graphicsQueue; }
    VkQueue getPresentQueue() { return presentQueue; }
    VkQueue getVideoQueue() { return videoQueue; }
    
    // queue indexes
    uint32_t getGraphicsQueueFamilyIndex() { return graphicsFamily.value(); };
    uint32_t getPresentQueueFamilyIndex() { return presentFamily.value(); };
    uint32_t getVideoQueueFamilyIndex() { return videoFamily.value(); };

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