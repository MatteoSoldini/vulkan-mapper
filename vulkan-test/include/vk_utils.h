#pragma once

#include <vulkan/vulkan_core.h>
#include <vector>
#include <GLFW/glfw3.h>
#include <optional>

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window);

std::optional<uint32_t> queryGraphicsQueueFamily(VkPhysicalDevice physicalDevice);
std::optional<uint32_t> queryPresentQueueFamily(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);
std::optional<uint32_t> queryVideoQueueFamily(VkPhysicalDevice physicalDevice);

VkImageView createImageView(VkDevice device, VkImage image, VkFormat format);
VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code);

uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);
void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
