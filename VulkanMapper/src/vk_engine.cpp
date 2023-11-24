#include <stdexcept>
#include <iostream>
#include <set>
#include <map>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_FORCE_RADIANS
#include <chrono>

#include "../include/vk_engine.h"
#include "../include/vk_types.h"
#include "../include/read_file.h"
#include "../include/clamp.h"
#include "../include/vk_objects.h"
#include "../include/scene.h"

#include <imgui.h>
#include "imgui_impl_glfw.h"
#include <imgui_impl_vulkan.h>

#include "../include/ui.h"
#include "../include/media_manager.h"
#include "../include/vk_video.h"


VulkanEngine::VulkanEngine() {
    // init volk
    if (volkInitialize() != VK_SUCCESS) {
        throw std::runtime_error("vulkan loader isn't installed on your system");
    }

    pScene = new Scene(this);
    pUi = new UI(this);
    pMediaManager = new MediaManager(this);
}

VkCommandBuffer VulkanEngine::beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void VulkanEngine::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void VulkanEngine::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    endSingleTimeCommands(commandBuffer);
}

bool VulkanEngine::checkValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : validationLayers) {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }

    return true;
}

void VulkanEngine::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto app = reinterpret_cast<VulkanEngine*>(glfwGetWindowUserPointer(window));
    app->framebufferResized = true;
}

void VulkanEngine::mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    auto app = reinterpret_cast<VulkanEngine*>(glfwGetWindowUserPointer(window));
    app->pScene->mouseButtonCallback(button, action, mods);
}

void VulkanEngine::initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // disable OpenGL context

    window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

    // create pScene
    std::cout << "init window" << std::endl;

    // resize callback
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);

    // cursor position callback
    //glfwSetCursorPosCallback(window, cursor_position_callback);

    // mouse button callback
    glfwSetMouseButtonCallback(window, mouse_button_callback);
}


void VulkanEngine::createInstance() {
    if (enableValidationLayers && !checkValidationLayerSupport()) { // check validation layers
        throw std::runtime_error("validation layers requested, but not available!");
    }

    VkApplicationInfo appInfo{}; // vulkan information
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Hello Triangle";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    // activate vulkan extension to interface with the window system
    uint32_t glfwExtensionCount = 0;

    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    createInfo.enabledExtensionCount = glfwExtensionCount;
    createInfo.ppEnabledExtensionNames = glfwExtensions;

    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    }
    else {
        createInfo.enabledLayerCount = 0;
    }

    //VkResult result = vkCreateInstance(&createInfo, nullptr, &instance); 

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) { // create instance
        throw std::runtime_error("failed to create instance!");
    }
    else {
        std::cout << "Vulkan instance created successfully" << std::endl;
    }

    volkLoadInstance(instance);

}

int VulkanEngine::rateDeviceSuitability(VkPhysicalDevice device) {
    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
    int score = 0;

    // Discrete GPUs have a significant performance advantage
    if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 1000;
    }

    // Maximum possible size of textures affects graphics quality
    score += deviceProperties.limits.maxImageDimension2D;


    // Minimum requirements

    // Check device supported queues
    auto graphicsFamily = queryGraphicsQueueFamily(device);
    auto presentFamily = queryPresentQueueFamily(device, surface);
    auto videoFamily = queryGraphicsQueueFamily(device);
    bool completeIndicies = graphicsFamily.has_value() && presentFamily.has_value() && videoFamily.has_value();
    
    // check extension
    bool extensionsSupported = checkDeviceExtensionSupport(device);

    // Swap chain support
    bool swapChainAdequate = false;
    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device, surface);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    // Also application can't function without geometry shaders

    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

    // vulkan 1.1 features (ycbcr sampler)
    VkPhysicalDeviceVulkan11Features supported11Features = {};
    supported11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;

    VkPhysicalDeviceFeatures2 supported2Features = {};
    supported2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    supported2Features.pNext = &supported11Features;
    supported2Features.features = supportedFeatures;

    vkGetPhysicalDeviceFeatures2(device, &supported2Features);

    if (!completeIndicies ||
        !extensionsSupported ||
        !deviceFeatures.geometryShader ||
        !swapChainAdequate ||
        !supportedFeatures.samplerAnisotropy ||
        !supportedFeatures.wideLines ||
        !supported11Features.samplerYcbcrConversion
        )
        return 0;

    return score;
}

bool VulkanEngine::checkDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    // print missing required extensions
    for (auto requiredExtension : requiredExtensions) {
        std::cout << "missing required extension: " << requiredExtension << std::endl;
    }

    return requiredExtensions.empty();
}

void VulkanEngine::createSurface() {
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }
    else {
        std::cout << "Window surface created successfully" << std::endl;
    }
}

void VulkanEngine::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    // Use an ordered map to automatically sort candidates by increasing score
    std::multimap<int, VkPhysicalDevice> candidates;

    for (const auto& device : devices) {
        int score = rateDeviceSuitability(device);
        candidates.insert(std::make_pair(score, device));
    }

    // Check if the best candidate is suitable at all
    if (candidates.rbegin()->first > 0) {
        physicalDevice = candidates.rbegin()->second;

        VkPhysicalDeviceProperties pProperties;
        vkGetPhysicalDeviceProperties(physicalDevice, &pProperties);
        std::cout << pProperties.deviceName << " has been selected with " << candidates.rbegin()->first << " score" << std::endl;
    }
    else {
        throw std::runtime_error("failed to find a suitable GPU!");
    }
}

void VulkanEngine::createLogicalDevice() {
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {
        graphicsFamily.value(),
        presentFamily.value(),
        videoFamily.value()
    };

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    deviceFeatures.wideLines = VK_TRUE;

    VkPhysicalDeviceVulkan11Features device11Features = {};
    device11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    device11Features.samplerYcbcrConversion = VK_TRUE;

    // Creating logical device
    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    }
    else {
        createInfo.enabledLayerCount = 0;
    }
    createInfo.pNext = &device11Features;

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error("failed to create logical device!");
    }
    else {
        std::cout << "Vulkan logical device created successfully" << std::endl;
    }

    vkGetDeviceQueue(device, graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, presentFamily.value(), 0, &presentQueue);
    vkGetDeviceQueue(device, videoFamily.value(), 0, &videoQueue);
}

void VulkanEngine::createSwapChain() {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice, surface);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities, window);

    // Simply sticking to this minimum means that we may sometimes have to wait on the driver to complete internal operations before we can acquire another image to render to. Therefore it is recommended to request at least one more image than the minimum
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;    // This is always 1 unless you are developing a stereoscopic 3D application
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // Specify how to handle swap chain images that will be used across multiple queue families
    uint32_t queueFamilyIndices[] = { graphicsFamily.value(), presentFamily.value() };

    if (graphicsFamily != presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;   // Images can be used across multiple queue families without explicit ownership transfers.
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;    // An image is owned by one queue family at a time and ownership must be explicitly transferred before using it in another queue family. This option offers the best performance.
        createInfo.queueFamilyIndexCount = 0; // Optional
        createInfo.pQueueFamilyIndices = nullptr; // Optional
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
        throw std::runtime_error("failed to create swap chain!");
    }
    else {
        std::cout << "Swap chain created successfully" << std::endl;
    }

    // We only specified a minimum number of images in the swap chain, so the implementation is allowed to create a swap chain with more. That's why we'll first query the final number of images with vkGetSwapchainImagesKHR, then resize the container and finally call it again to retrieve the handles
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());
    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;
}

void VulkanEngine::queryQueueFamilies() {
    graphicsFamily = queryGraphicsQueueFamily(physicalDevice);
    presentFamily = queryPresentQueueFamily(physicalDevice, surface);
    videoFamily = queryVideoQueueFamily(physicalDevice);
}

void VulkanEngine::createImageViews() {
    // Creates a basic image view for every image in the swap chain

    swapChainImageViews.resize(swapChainImages.size());

    for (uint32_t i = 0; i < swapChainImages.size(); i++) {
        swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat, nullptr);
    }
}

void VulkanEngine::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;
    /*
    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }
    */
}

void VulkanEngine::loadPipelines() {
    for (int i = 0; i < pipelinesToLoad.size(); i++) {
        // load shaders
        auto vertShaderCode = readFile(pipelinesToLoad[i].vertexShaderFile);
        auto fragShaderCode = readFile(pipelinesToLoad[i].fragmentShaderFile);

        // create shaders
        VkShaderModule vertShaderModule = createShaderModule(device, vertShaderCode);
        VkShaderModule fragShaderModule = createShaderModule(device, fragShaderCode);

        // add shader to pipeline
        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main"; // entry point

        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

        // pipeline dynamic state
        // a limited amount of the state can actually be changed without recreating the pipeline at draw time
        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
            VK_DYNAMIC_STATE_LINE_WIDTH,
        };

        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        // create pipeline state for vertex shader
        auto bindingDescription = Vertex::getBindingDescription();
        auto attributeDescriptions = Vertex::getAttributeDescriptions();

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        // input assembly
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = pipelinesToLoad[i].drawTopology;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        // create viewport
        // trasformation
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)swapChainExtent.width;
        viewport.height = (float)swapChainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        // crop
        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = swapChainExtent;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        // create rasterizer
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE; // disables any output to the framebuffer
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f; // Optional
        rasterizer.depthBiasClamp = 0.0f; // Optional
        rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

        // create multisampling (one of the ways to perform anti-aliasing)
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.0f; // Optional
        multisampling.pSampleMask = nullptr; // Optional
        multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
        multisampling.alphaToOneEnable = VK_FALSE; // Optional

        // create color blending 
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;    // if disabled the new color goes into the framebuffer without blending
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f; // Optional
        colorBlending.blendConstants[1] = 0.0f; // Optional
        colorBlending.blendConstants[2] = 0.0f; // Optional
        colorBlending.blendConstants[3] = 0.0f; // Optional

        std::vector<VkDescriptorSetLayout> layouts;
        layouts.resize(pipelinesToLoad[i].descriptorSetLayouts.size());
        for (int j = 0; j < layouts.size(); j++) {
            layouts[j] = *(pipelinesToLoad[i].descriptorSetLayouts[j]);
        }

        // create pipeline layout
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = layouts.size();
        pipelineLayoutInfo.pSetLayouts = layouts.data();

        VkPipelineLayout pipelineLayout;

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = nullptr; // Optional
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = pipelineLayout;   // fixed-function stage
        pipelineInfo.renderPass = viewportRenderpass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
        pipelineInfo.basePipelineIndex = -1; // Optional

        VkPipeline pipeline;

        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }
        else {
            std::cout << "Graphic pipeline created successfully" << std::endl;
        }

        // clean up
        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);

        Pipeline new_pipeline{};
        new_pipeline.pipeline = pipeline;
        new_pipeline.pipelineLayout = pipelineLayout;

        pipelines.emplace(pipelinesToLoad[i].name, new_pipeline);
    }
}

void VulkanEngine::createFramebuffers(std::vector<VkFramebuffer>& frameBuffers, VkRenderPass renderPass) {
    frameBuffers.resize(swapChainImageViews.size());
    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        VkImageView attachments[] = {
            swapChainImageViews[i]
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &frameBuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }
        else {
            std::cout << "Framebuffer " << i << " created successfully" << std::endl;
        }
    }
}

void VulkanEngine::createCommandPools() {
    // graphics command pool
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = graphicsFamily.value();

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }

    // graphics command buffer
    commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

    if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }

    // video command pool
    poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = videoFamily.value();

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &videoCommandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }

    allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = videoCommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(device, &allocInfo, &videoCommandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
}

void VulkanEngine::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0; // Optional
    beginInfo.pInheritanceInfo = nullptr; // Optional

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = viewportRenderpass;
    renderPassInfo.framebuffer = viewportFramebuffers[currentFrame];
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent.width = viewportTextures[currentFrame].width;
    renderPassInfo.renderArea.extent.height = viewportTextures[currentFrame].height;

    VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(viewportTextures[currentFrame].width);
    viewport.height = static_cast<float>(viewportTextures[currentFrame].height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = swapChainExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    // bind buffer
    VkBuffer vertexBuffers[] = { vertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);

    // looping objects
    std::string lastPipelineName = "";

    auto object_ids = pScene->getIds();
    uint32_t indexOffset = 0;

    for (auto object_id : object_ids) {
        auto object = pScene->getObjectPointer(object_id);

        // bind new pipeline if needed
        std::string pipelineName = object->getPipelineName();
        if (pipelineName != lastPipelineName) {
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[pipelineName].pipeline);

            // bind vertex and index buffer to new pipeline
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[pipelineName].pipelineLayout, 0, 1, &uniformBufferSets[currentFrame], 0, nullptr);
            lastPipelineName = pipelineName;
        }

        // bind texture
        if (pipelineName == "texture") {
            auto plane = dynamic_cast<Plane*>(object);
            if (plane != nullptr) {
                Texture texture = textures[plane->getMediaId()];
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[pipelineName].pipelineLayout, 1, 1, &texture.descriptorSet, 0, nullptr);
            }
        }

        // bind video frame
        if (pipelineName == "video_frame") {
            // TODO: bind if necessary
            //if (videoFrameView != prevVideoFrameView) {
            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = videoFrameView;
            imageInfo.sampler = ycbcrFrameSampler;

            VkWriteDescriptorSet descriptorWrites{};
            descriptorWrites.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites.dstSet = videoFrameDescriptorSet[currentFrame];
            descriptorWrites.dstBinding = 0;
            descriptorWrites.dstArrayElement = 0;
            descriptorWrites.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites.descriptorCount = 1;
            descriptorWrites.pImageInfo = &imageInfo;
            descriptorWrites.pNext = nullptr;

            vkUpdateDescriptorSets(device, 1, &descriptorWrites, 0, nullptr);   // executed immediately

            prevVideoFrameView = videoFrameView;
            //}

            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[pipelineName].pipelineLayout, 1, 1, &videoFrameDescriptorSet[currentFrame], 0, nullptr);
        }

        if (pipelineName == "line") {
            vkCmdSetLineWidth(commandBuffer, 4.0f);
        }

        // draw
        std::vector<uint16_t> object_indices = object->getIndices();
        uint32_t indexCount = object_indices.size();
        vkCmdDrawIndexed(commandBuffer, indexCount, 1, indexOffset, 0, 0);
        indexOffset += object_indices.size();
    }

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }
}

void VulkanEngine::createSyncObjects() {
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {

            throw std::runtime_error("Failed to create synchronization objects for a frame!");
        }
    }

    std::cout << "Synchronization objects created successfully" << std::endl;
}

void VulkanEngine::recreateSwapChain() {
    // wait if window is minimized
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(device);

    cleanupSwapChain();

    createSwapChain();
    createImageViews();
    
    //createFramebuffers(swapChainFramebuffers, renderPass);

    // imgui
    for (size_t i = 0; i < imGuiFrameBuffers.size(); i++) {
        vkDestroyFramebuffer(device, imGuiFrameBuffers[i], nullptr);
    }
    createFramebuffers(imGuiFrameBuffers, imGuiRenderPass);

    //ImGui_ImplVulkanH_CreateOrResizeWindow(g_Instance, g_PhysicalDevice, g_Device, &g_MainWindowData, g_QueueFamily, g_Allocator, width, height, g_MinImageCount);
}

void VulkanEngine::recreateViewportSurface(uint32_t width, uint32_t height, uint32_t surfaceIndex) {
    // destroy outdated surface
    vkDestroyFramebuffer(device, viewportFramebuffers[surfaceIndex], nullptr);
    vkDestroyImageView(device, viewportTextures[surfaceIndex].imageView, nullptr);
    vkDestroyImage(device, viewportTextures[surfaceIndex].image, nullptr);
    vkFreeMemory(device, viewportTextures[surfaceIndex].imageMemory, nullptr);

    // create destination image
    VkImage image;
    VkDeviceMemory imageMemory;
    createImage(width, height, 1, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, imageMemory, nullptr);

    transitionImageLayout(image, VK_FORMAT_R8G8B8A8_SRGB, 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // create image view
    VkImageView imageView = createImageView(image, VK_FORMAT_R8G8B8A8_SRGB, nullptr);

    // create framebuffer
    VkImageView attachments[] = {
        imageView
    };

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = viewportRenderpass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = attachments;
    framebufferInfo.width = width;
    framebufferInfo.height = height;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &viewportFramebuffers[surfaceIndex]) != VK_SUCCESS) {
        throw std::runtime_error("failed to create framebuffer!");
    }

    Texture texture{};
    texture.image = image;
    texture.imageMemory = imageMemory;
    texture.imageView = imageView;
    texture.descriptorSet = ImGui_ImplVulkan_AddTexture(textureSampler, imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    texture.width = width;
    texture.height = height;

    viewportTextures[surfaceIndex] = texture;

    std::cout << "Viewport surface " << currentFrame << " recreated successfully" << std::endl;
}

void VulkanEngine::transitionImageLayout(VkImage image, VkFormat format, uint32_t layerCount, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = layerCount;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    endSingleTimeCommands(commandBuffer);
}

VkImageView VulkanEngine::createImageView(VkImage image, VkFormat format, void* pNext) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    viewInfo.pNext = pNext;

    VkImageView imageView;
    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture image view!");
    }

    return imageView;
}

void VulkanEngine::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = {
        width,
        height,
        1
    };

    vkCmdCopyBufferToImage(
        commandBuffer,
        buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );

    endSingleTimeCommands(commandBuffer);
}

void VulkanEngine::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory, void* pNext) {
    // Buffer creation
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    bufferInfo.pNext = pNext;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create buffer!");
    }

    // Memory requirements
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    // Memory allocation
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate buffer memory!");
    }


    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

void VulkanEngine::createImage(uint32_t width, uint32_t height, uint32_t arrayLayers, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory, const void* pNext) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = arrayLayers;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.pNext = pNext;

    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory!");
    }

    vkBindImageMemory(device, image, imageMemory, 0);
}

/*
void VulkanEngine::createTextureImage() {
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load("MSC_pic.png", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    VkDeviceSize imageSize = texWidth * texHeight * 4;

    if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
    }

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(device, stagingBufferMemory);

    stbi_image_free(pixels);

    createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture_image, texture_image_memory);

    transitionImageLayout(texture_image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(stagingBuffer, texture_image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

    transitionImageLayout(texture_image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}
*/

void VulkanEngine::createSamplers() {
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);

    // texture sampler
    {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        if (vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture sampler!");
        }
    }
    
    // ycbcr frame sampler
    {
        VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 16.0f;

        // sampler conversion
        VkSamplerYcbcrConversionCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO;
        createInfo.format = FRAME_FORMAT;

        // Which 3x3 YUV to RGB matrix is used?
        // 601 is generally used for SD content.
        // 709 for HD content.
        // 2020 for UHD content.
        createInfo.ycbcrModel = VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_709;

        // TV (NARROW) or PC (FULL) range for YUV?
        // Usually, JPEG uses full range and broadcast content is narrow.
        // If using narrow, the YUV components need to be
        // rescaled before it can be converted.
        createInfo.ycbcrRange = VK_SAMPLER_YCBCR_RANGE_ITU_NARROW;

        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        createInfo.xChromaOffset = VK_CHROMA_LOCATION_MIDPOINT;
        createInfo.yChromaOffset = VK_CHROMA_LOCATION_MIDPOINT;

        createInfo.chromaFilter = VK_FILTER_LINEAR;
        createInfo.forceExplicitReconstruction = false;
        createInfo.pNext = nullptr;

        vkCreateSamplerYcbcrConversion(device, &createInfo, nullptr, &ycbcrSamplerConversion);

        VkSamplerYcbcrConversionInfo conversionInfo = {};
        conversionInfo.conversion = ycbcrSamplerConversion;
        conversionInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO_KHR;
        conversionInfo.pNext = nullptr;

        samplerInfo.pNext = &conversionInfo;

        if (vkCreateSampler(device, &samplerInfo, nullptr, &ycbcrFrameSampler) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture sampler!");
        }
    }
}

void VulkanEngine::createVertexBuffer() {
    int vertices_size = 0;
    std::vector<Vertex> vertices;
    
    auto object_ids = pScene->getIds();

    for (auto object_id : object_ids) {
        std::vector<Vertex> obj_vertices = pScene->getObjectPointer(object_id)->getVertices();

        vertices_size += obj_vertices.size() * sizeof(obj_vertices[0]);
        vertices.insert(vertices.end(), obj_vertices.begin(), obj_vertices.end());
    }

    // create buffer
    VkDeviceSize bufferSize = sizeof(Vertex) * VERTICES_COUNT;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory, nullptr);

    // return if no data to transfer
    if (vertices_size == 0) return;

    // transfer vertices
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory, nullptr);

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), (size_t)bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void VulkanEngine::createIndexBuffer() {
    int indices_size = 0;
    std::vector<uint16_t> indices;
    
    int offset = 0;
    auto object_ids = pScene->getIds();

    for (auto object_id : object_ids) {
        std::vector<uint16_t> obj_indices = pScene->getObjectPointer(object_id)->getIndices();

        // sum offset
        for (auto index = obj_indices.begin(); index != obj_indices.end(); index++) {
            *index += offset;
        }

        indices_size += obj_indices.size() * sizeof(obj_indices[0]);
        indices.insert(indices.end(), obj_indices.begin(), obj_indices.end());

        offset += pScene->getObjectPointer(object_id)->getVertices().size();
    }

    // create buffer
    VkDeviceSize bufferSize = sizeof(uint16_t) * INDICES_COUNT;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory, nullptr);

    // return if no data to transfer
    if (indices_size == 0) return;

    // transfer indices
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory, nullptr);

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, indices.data(), (size_t)bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);


    copyBuffer(stagingBuffer, indexBuffer, bufferSize);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void VulkanEngine::createDescriptorSetLayouts() {
    // uniform buffer layout
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    uboLayoutBinding.pImmutableSamplers = nullptr; // Optional
    
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboLayoutBinding;

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &uniformBufferLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }

    // texture layout
    VkDescriptorSetLayoutBinding textureLayoutBinding{};
    textureLayoutBinding.binding = 0;
    textureLayoutBinding.descriptorCount = 1;
    textureLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    textureLayoutBinding.pImmutableSamplers = nullptr;
    textureLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &textureLayoutBinding;

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &textureLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }

    // video frame layout
    VkDescriptorSetLayoutBinding videoFrameLayoutBinding{};
    videoFrameLayoutBinding.binding = 0;
    videoFrameLayoutBinding.descriptorCount = 1;
    videoFrameLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    videoFrameLayoutBinding.pImmutableSamplers = &ycbcrFrameSampler;
    videoFrameLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &videoFrameLayoutBinding;

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &videoFrameLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }
}

void VulkanEngine::createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i], nullptr);

        vkMapMemory(device, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
    }
}

void VulkanEngine::createDescriptorPool() {
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    if (vkCreateDescriptorPool(device, &pool_info, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }
}

void VulkanEngine::createStaticDescriptorSets() {
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, uniformBufferLayout);

    {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        allocInfo.pSetLayouts = layouts.data();

        uniformBufferSets.resize(MAX_FRAMES_IN_FLIGHT);
        if (vkAllocateDescriptorSets(device, &allocInfo, uniformBufferSets.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }
    }

    // binding
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        /*
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = textureImageView;
        imageInfo.sampler = textureSampler;
        */
        
        std::array<VkWriteDescriptorSet, 1> descriptorWrites{};

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = uniformBufferSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;
        
        /*
        descriptor_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[1].dstSet = uniformBufferSets[i];
        descriptor_writes[1].dstBinding = 1;
        descriptor_writes[1].dstArrayElement = 0;
        descriptor_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor_writes[1].descriptorCount = 1;
        descriptor_writes[1].pImageInfo = &imageInfo;
        */

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }

    // decoded frame view
    {
        videoFrameDescriptorSet.resize(MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = descriptorPool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = &videoFrameLayout;

            if (vkAllocateDescriptorSets(device, &allocInfo, &videoFrameDescriptorSet[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to allocate descriptor sets!");
            }
        }
    }
}

void VulkanEngine::initViewportRender() {
    VkFormat image_format = VK_FORMAT_R8G8B8A8_SRGB;   // don't know

    // create renderpass
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = image_format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &viewportRenderpass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }

    viewportFramebuffers.resize(MAX_FRAMES_IN_FLIGHT);
    viewportTextures.resize(MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        recreateViewportSurface(swapChainExtent.width, swapChainExtent.height, i);
    }
}

void VulkanEngine::cleanupViewportRender() {
    // destroy renderpass
    vkDestroyRenderPass(device, viewportRenderpass, nullptr);
    // destroy surfaces
    for (auto texture : viewportTextures) {
        vkDestroyImageView(device, texture.imageView, nullptr);
        vkDestroyImage(device, texture.image, nullptr);
        vkFreeMemory(device, texture.imageMemory, nullptr);
    }
    for (auto framebuffer : viewportFramebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
}

void VulkanEngine::initVulkan() {
    // instance
    createInstance();
    createSurface();
    
    // device
    pickPhysicalDevice();
    queryQueueFamilies();
    createLogicalDevice();
    
    // presentation
    createSwapChain();
    createImageViews();
    createSyncObjects();

    // ui
    initImGui();

    // offscreen rendering
    createSamplers();
    //createRenderPass();
    createDescriptorSetLayouts();

    //createFramebuffers(swapChainFramebuffers, renderPass);
    createCommandPools();
    createVertexBuffer();
    createIndexBuffer();
    createUniformBuffers();
    createDescriptorPool();
    createStaticDescriptorSets();

    initViewportRender();
    loadPipelines();

    // output window
    VulkanEngine::pOutput = new VulkanOutput({
        instance,
        device,
        graphicsFamily.value(),
        presentFamily.value(),
        graphicsQueue,
        presentQueue,
        physicalDevice,
        vertexBuffer,
        indexBuffer,
        &textures,
        uniformBufferLayout,
        textureLayout,
        pScene,
    }, this);

    // TEMP
    //pMediaManager->loadFile("video_test.mp4");
}

void VulkanEngine::updateUniformBuffer(uint32_t currentImage) {
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    float aspect = (float)width / (float)height;

    UniformBufferObject ubo{};
    ubo.model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));
    ubo.view = glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    //ubo.proj = glm::ortho(-.5f* aspect, .5f* aspect, -.5f, .5f, -10.0f, 10.0f);
    ubo.proj = glm::perspective(glm::radians(60.0f), viewportTextures[currentFrame].width / (float)viewportTextures[currentFrame].height, 0.0f, 10.0f);
    ubo.proj[1][1] *= -1;

    memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}

void VulkanEngine::updateVertexBuffer() {
    /*
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
    */

    // generate vertex vector to transfer
    int vertices_size = 0;
    std::vector<Vertex> vertices;

    auto object_ids = pScene->getIds();
    for (auto object_id : object_ids) {
        std::vector<Vertex> obj_vertices = pScene->getObjectPointer(object_id)->getVertices();

        vertices_size += obj_vertices.size() * sizeof(obj_vertices[0]);
        vertices.insert(vertices.end(), obj_vertices.begin(), obj_vertices.end());
    }

    if (vertices_size == 0) return;

    VkDeviceSize bufferSize = vertices_size;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory, nullptr);

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), (size_t)bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void VulkanEngine::updateIndexBuffer() {
    int indices_size = 0;
    std::vector<uint16_t> indices;

    int offset = 0;
    auto object_ids = pScene->getIds();
    for (auto object_id : object_ids) {
        std::vector<uint16_t> obj_indices = pScene->getObjectPointer(object_id)->getIndices();

        // sum offset
        for (auto index = obj_indices.begin(); index != obj_indices.end(); index++) {
            *index += offset;
        }

        indices_size += obj_indices.size() * sizeof(obj_indices[0]);
        indices.insert(indices.end(), obj_indices.begin(), obj_indices.end());

        offset += pScene->getObjectPointer(object_id)->getVertices().size();
    }

    if (indices_size == 0) return;

    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory, nullptr);

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, indices.data(), (size_t)bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    copyBuffer(stagingBuffer, indexBuffer, bufferSize);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void VulkanEngine::drawFrame() {
    // wait for previous frame
    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    // acquiring an image from the swap chain
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    vkResetFences(device, 1, &inFlightFences[currentFrame]);

    vkResetCommandBuffer(commandBuffers[currentFrame], 0);

    // imgui
    recordImGuiCommandBuffer(imageIndex);

    // submit command buffer
    std::array<VkCommandBuffer, 2> submitCommandBuffers = { 
        commandBuffers[currentFrame],
        imGuiCommandBuffers[currentFrame]
    };

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = static_cast<uint32_t>(submitCommandBuffers.size());
    submitInfo.pCommandBuffers = submitCommandBuffers.data();

    VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    // presentation
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    VkSwapchainKHR swapChains[] = { swapChain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr; // Optional

    vkQueuePresentKHR(presentQueue, &presentInfo);

    // advance frame
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}


void VulkanEngine::mainLoop() {
    while (!glfwWindowShouldClose(window)) { // to keep the application running until either an error occurs or the window is closed
        // window events
        glfwPollEvents();

        pMediaManager->decodeFrames();

        drawFrame();

        // output
        if (showOutput) {
            pOutput->drawFrame();
        }
    }

    vkDeviceWaitIdle(device);
}

void VulkanEngine::cleanupSwapChain() {
    /*
    * for (size_t i = 0; i < swapChainFramebuffers.size(); i++) {
        vkDestroyFramebuffer(device, swapChainFramebuffers[i], nullptr);
    }
    */

    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        vkDestroyImageView(device, swapChainImageViews[i], nullptr);
    }

    vkDestroySwapchainKHR(device, swapChain, nullptr);
}

void VulkanEngine::cleanup() {
    cleanupSwapChain();

    // destroy viewport render
    cleanupViewportRender();

    // imgui
    imGuiCleanup();

    vkDestroySampler(device, textureSampler, nullptr);

    // destroy textures
    for (auto texture : textures) {
        vkDestroyImageView(device, texture.second.imageView, nullptr);
        vkDestroyImage(device, texture.second.image, nullptr);
        vkFreeMemory(device, texture.second.imageMemory, nullptr);
    }

    // Destroy uniform buffers
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyBuffer(device, uniformBuffers[i], nullptr);
        vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
    }

    // Destroy descriptor pools
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);

    // destroy descriptor set layouts
    vkDestroyDescriptorSetLayout(device, uniformBufferLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, textureLayout, nullptr);

    vkDestroyBuffer(device, indexBuffer, nullptr);
    vkFreeMemory(device, indexBufferMemory, nullptr);

    vkDestroyBuffer(device, vertexBuffer, nullptr);
    vkFreeMemory(device, vertexBufferMemory, nullptr);

    for (auto pipeline : pipelines) {
        vkDestroyPipeline(device, pipeline.second.pipeline, nullptr);
        vkDestroyPipelineLayout(device, pipeline.second.pipelineLayout, nullptr);
    }
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(device, inFlightFences[i], nullptr);
    }

    vkDestroyCommandPool(device, commandPool, nullptr);

    vkDestroyDevice(device, nullptr);

    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);

    glfwDestroyWindow(window);

    glfwTerminate();
}

void VulkanEngine::initImGui() {
    // create descriptor pool
    // the size of the pool is very oversize, but it's copied from imgui demo itself.
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo descriptorInfo = {};
    descriptorInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    descriptorInfo.maxSets = 1000;
    descriptorInfo.poolSizeCount = std::size(poolSizes);
    descriptorInfo.pPoolSizes = poolSizes;

    if (vkCreateDescriptorPool(device, &descriptorInfo, nullptr, &imguiDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }

    // command pool
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = graphicsFamily.value();

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &imGuiCommandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }

    // create command buffer
    imGuiCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = imGuiCommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = imGuiCommandBuffers.size();

    if (vkAllocateCommandBuffers(device, &allocInfo, imGuiCommandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }

    // create render pass
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;    // clear before drawing
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;  // necessary for the first render pass
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;  // translate to layout for presentation

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &imGuiRenderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }

    // create framebuffers
    imGuiFrameBuffers.resize(swapChainImageViews.size());
    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        VkImageView attachments[] = {
            swapChainImageViews[i]
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = imGuiRenderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &imGuiFrameBuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }
        else {
            std::cout << "ImGui framebuffer " << i << " created successfully" << std::endl;
        }
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    ImGui_ImplVulkan_LoadFunctions([](const char* function_name, void* vulkan_instance) {
        return vkGetInstanceProcAddr(*(reinterpret_cast<VkInstance*>(vulkan_instance)), function_name);
    }, & instance);

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForVulkan(window, true);
    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.Instance = instance;
    initInfo.PhysicalDevice = physicalDevice;
    initInfo.Device = device;
    initInfo.QueueFamily = 0;  // not using
    initInfo.Queue = graphicsQueue;
    initInfo.PipelineCache = nullptr;  // not using
    initInfo.DescriptorPool = imguiDescriptorPool;
    initInfo.Subpass = 0;
    initInfo.MinImageCount = MAX_FRAMES_IN_FLIGHT;
    initInfo.ImageCount = MAX_FRAMES_IN_FLIGHT;
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.Allocator = nullptr;
    initInfo.CheckVkResultFn = nullptr;
    ImGui_ImplVulkan_Init(&initInfo, imGuiRenderPass);
        
    // fonts loading
    
    // Use any command queue
    VkCommandPool command_pool = imGuiCommandPool;
    VkCommandBuffer command_buffer = imGuiCommandBuffers[0];    // any

    if (vkResetCommandPool(device, command_pool, 0)) {
        throw std::runtime_error("failed to reset command pool");
    }
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(command_buffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer");
    }

    VkSubmitInfo end_info = {};
    end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    end_info.commandBufferCount = 1;
    end_info.pCommandBuffers = &command_buffer;
    if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to end recording command buffer");
    }
    
    vkQueueSubmit(graphicsQueue, 1, &end_info, VK_NULL_HANDLE);

    vkDeviceWaitIdle(device);
}

void VulkanEngine::recordImGuiCommandBuffer(uint32_t imageIndex) {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    pUi->drawUi();

    ImGui::Render();

    vkResetCommandBuffer(imGuiCommandBuffers[currentFrame], 0);
    //vkResetCommandPool(device, imGuiCommandPool, 0);

    VkCommandBufferBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    if (vkBeginCommandBuffer(imGuiCommandBuffers[currentFrame], &info) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording command buffer");
    }
    
    {
        VkRenderPassBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass = imGuiRenderPass;
        info.framebuffer = imGuiFrameBuffers[imageIndex];
        info.renderArea.offset = { 0, 0 };
        info.renderArea.extent = swapChainExtent;

        VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
        info.clearValueCount = 1;
        info.pClearValues = &clearColor;

        vkCmdBeginRenderPass(imGuiCommandBuffers[currentFrame], &info, VK_SUBPASS_CONTENTS_INLINE);
    }
    

    // Record dear imgui primitives into command buffer
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), imGuiCommandBuffers[currentFrame]);

    // Submit command buffer
    vkCmdEndRenderPass(imGuiCommandBuffers[currentFrame]);
    if (vkEndCommandBuffer(imGuiCommandBuffers[currentFrame])) {
        throw std::runtime_error("Failed to end recording command buffer");
    }
}

void VulkanEngine::imGuiCleanup() {
    for (size_t i = 0; i < imGuiFrameBuffers.size(); i++) {
        vkDestroyFramebuffer(device, imGuiFrameBuffers[i], nullptr);
    }
    
    vkDestroyRenderPass(device, imGuiRenderPass, nullptr);

    vkDestroyCommandPool(device, imGuiCommandPool, nullptr);

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    vkDestroyDescriptorPool(device, imguiDescriptorPool, nullptr);
}

void VulkanEngine::loadTexture(uint8_t id, unsigned char* pixels, int width, int height) {
    VkDeviceSize imageSize = width * height * 4;

    // transfer image
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory, nullptr);

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(device, stagingBufferMemory);

    VkImage texture_image;
    VkDeviceMemory texture_image_memory;

    createImage(width, height, 1, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture_image, texture_image_memory, nullptr);

    transitionImageLayout(texture_image, VK_FORMAT_R8G8B8A8_SRGB, 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(stagingBuffer, texture_image, static_cast<uint32_t>(width), static_cast<uint32_t>(height));

    transitionImageLayout(texture_image, VK_FORMAT_R8G8B8A8_SRGB, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);

    // create descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &textureLayout;

    VkDescriptorSet descriptor_set;
    if (vkAllocateDescriptorSets(device, &allocInfo, &descriptor_set) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    VkImageView image_view = createImageView(texture_image, VK_FORMAT_R8G8B8A8_SRGB, nullptr);

    // binding
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = image_view;
    imageInfo.sampler = textureSampler;

    VkWriteDescriptorSet descriptor_writes{};
    descriptor_writes.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_writes.dstSet = descriptor_set;
    descriptor_writes.dstBinding = 0;
    descriptor_writes.dstArrayElement = 0;
    descriptor_writes.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptor_writes.descriptorCount = 1;
    descriptor_writes.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(device, 1, &descriptor_writes, 0, nullptr);

    Texture new_texture{};
    new_texture.image = texture_image;
    new_texture.imageMemory = texture_image_memory;
    new_texture.imageView = image_view;
    new_texture.descriptorSet = descriptor_set;
    new_texture.width = width;
    new_texture.height = height;

    textures.emplace(id, new_texture);
}

bool VulkanEngine::getShowOutput() {
    return showOutput;
}

void VulkanEngine::setShowOutput(bool showOutput) {
    VulkanEngine::showOutput = showOutput;
    
    if (showOutput) {
        pOutput->init();
    }
    else {
        pOutput->close();
    }
}

VkDescriptorSet VulkanEngine::renderViewport(uint32_t viewportWidth, uint32_t viewportHeight, uint32_t cursorPosX, uint32_t cursorPosY) {
    // pass cursor position to scene
    
    // Transform NDC to camera-space coordinate
    // inverse view transformation not needed since camera is fixed

    // make cursor coordinates from -1 to +1
    float ndcX = ((float)cursorPosX / viewportWidth) * 2.f - 1.f;
    float ndcY = -((float)cursorPosY / viewportHeight) * 2.f + 1.f;

    // inverse of projection matrix
    glm::vec4 clipCoords = { ndcX, ndcY, -1.0f, 1.0f };
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), viewportWidth / (float)viewportHeight, 0.1f, 10.0f);
    glm::mat4 projInv = glm::inverse(proj);
    glm::vec4 cameraCoords = projInv * clipCoords;
    glm::mat4 viewInv = glm::inverse(glm::lookAt(glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::vec4 mouseRay = glm::normalize(cameraCoords * viewInv);
    pScene->mouseRayCallback(mouseRay);
    
    // resize surfaces before rendering
    if (viewportTextures[currentFrame].width != viewportWidth || viewportTextures[currentFrame].height != viewportHeight) {
        recreateViewportSurface(viewportWidth, viewportHeight, currentFrame);
    }
    
    // update data
    updateVertexBuffer();
    updateIndexBuffer();
    updateUniformBuffer(currentFrame);

    // render
    recordCommandBuffer(commandBuffers[currentFrame], currentFrame);

    return viewportTextures[currentFrame].descriptorSet;
}

void VulkanEngine::loadVideoFrame(VkImageView imageView) {
    videoFrameView = imageView;

    // binding
    /*VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = imageView;
    imageInfo.sampler = ycbcrFrameSampler;
    
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkWriteDescriptorSet descriptorWrites{};
        descriptorWrites.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites.dstSet = videoFrameViews[i];
        descriptorWrites.dstBinding = 0;
        descriptorWrites.dstArrayElement = 0;
        descriptorWrites.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites.descriptorCount = 1;
        descriptorWrites.pImageInfo = &imageInfo;
        descriptorWrites.pNext = nullptr;

        vkUpdateDescriptorSets(device, 1, &descriptorWrites, 0, nullptr);
    }*/
}
