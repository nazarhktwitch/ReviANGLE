// Vulkan proxy layer implementation
// Routes OpenGL calls through Vulkan instead of ANGLE/DirectX

#include "vk_proxy.hpp"
#include "angle_loader.hpp"
#include "config.hpp"
#include <cstring>
#include <algorithm>
#include <iostream>

namespace vkproxy {

// Singleton instance
static VulkanState* g_vulkanState = nullptr;

VulkanState& VulkanState::getInstance() {
    if (!g_vulkanState) {
        g_vulkanState = new VulkanState();
    }
    return *g_vulkanState;
}

bool VulkanState::initialize() {
    if (instance != VK_NULL_HANDLE) {
        return true; // Already initialized
    }

    // Create Vulkan instance
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "ReviANGLE";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 1, 0);
    appInfo.pEngineName = "ReviANGLE";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    // Required extensions for Windows
    const char* instanceExtensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
    };

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = 2;
    createInfo.ppEnabledExtensionNames = instanceExtensions;

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        angle::log("vk_proxy: Failed to create Vulkan instance");
        return false;
    }

    angle::log("vk_proxy: Vulkan instance created");

    // Enumerate physical devices
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    
    if (deviceCount == 0) {
        angle::log("vk_proxy: No Vulkan physical devices found");
        vkDestroyInstance(instance, nullptr);
        instance = VK_NULL_HANDLE;
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
    physicalDevice = devices[0]; // Select first device (usually the GPU)

    // Get physical device properties
    VkPhysicalDeviceProperties deviceProps;
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProps);
    angle::log("vk_proxy: Selected GPU: %s", deviceProps.deviceName);

    // Find queue families
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

    graphicsQueueFamily = UINT32_MAX;
    presentQueueFamily = UINT32_MAX;

    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphicsQueueFamily = i;
            break;
        }
    }

    if (graphicsQueueFamily == UINT32_MAX) {
        angle::log("vk_proxy: No graphics queue family found");
        vkDestroyInstance(instance, nullptr);
        instance = VK_NULL_HANDLE;
        return false;
    }

    presentQueueFamily = graphicsQueueFamily; // Simplified: use same queue

    // Create logical device
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = graphicsQueueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    const char* deviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    VkPhysicalDeviceFeatures deviceFeatures{};

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
    deviceCreateInfo.enabledExtensionCount = 1;
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;

    if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS) {
        angle::log("vk_proxy: Failed to create Vulkan device");
        vkDestroyInstance(instance, nullptr);
        instance = VK_NULL_HANDLE;
        return false;
    }

    // Get queue handles
    vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);
    presentQueue = graphicsQueue;

    angle::log("vk_proxy: Vulkan device initialized successfully");
    return true;
}

void VulkanState::shutdown() {
    if (device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);
        vkDestroyDevice(device, nullptr);
        device = VK_NULL_HANDLE;
    }

    if (instance != VK_NULL_HANDLE) {
        vkDestroyInstance(instance, nullptr);
        instance = VK_NULL_HANDLE;
    }

    angle::log("vk_proxy: Vulkan shutdown complete");
}

std::unique_ptr<VulkanContext> VulkanState::createContext(HDC hdc, HWND hwnd) {
    if (!initialize()) {
        return nullptr;
    }

    auto ctx = std::make_unique<VulkanContext>();
    ctx->device = device;
    ctx->queue = graphicsQueue;
    ctx->hdc = hdc;
    ctx->hwnd = hwnd;

    // Create command pool
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = graphicsQueueFamily;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &ctx->cmdPool) != VK_SUCCESS) {
        angle::log("vk_proxy: Failed to create command pool");
        return ctx;
    }

    if (!hwnd) {
        angle::log("vk_proxy: Dummy Vulkan context created (no HWND)");
        return ctx;
    }

    // Create Windows surface using vkCreateWin32SurfaceKHR
    VkWin32SurfaceCreateInfoKHR surfaceCreateInfo{};
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.hwnd = hwnd;
    surfaceCreateInfo.hinstance = GetModuleHandleA(nullptr);

    PFN_vkCreateWin32SurfaceKHR vkCreateWin32SurfaceKHR =
        reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(
            vkGetInstanceProcAddr(instance, "vkCreateWin32SurfaceKHR"));

    if (!vkCreateWin32SurfaceKHR) {
        angle::log("vk_proxy: vkCreateWin32SurfaceKHR not available");
        return ctx;
    }

    if (vkCreateWin32SurfaceKHR(instance, &surfaceCreateInfo, nullptr, &ctx->surface) != VK_SUCCESS) {
        angle::log("vk_proxy: Failed to create Win32 surface for hwnd %p", hwnd);
        return ctx;
    }

    // Create swapchain
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, ctx->surface, &surfaceCapabilities);

    VkSwapchainCreateInfoKHR swapchainCreateInfo{};
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.surface = ctx->surface;
    uint32_t desiredImageCount = std::max(2u, surfaceCapabilities.minImageCount);
    if (surfaceCapabilities.maxImageCount > 0) {
        desiredImageCount = std::min(desiredImageCount, surfaceCapabilities.maxImageCount);
    }
    swapchainCreateInfo.minImageCount = desiredImageCount;
    swapchainCreateInfo.imageFormat = VK_FORMAT_B8G8R8A8_SRGB;
    swapchainCreateInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    swapchainCreateInfo.imageExtent = surfaceCapabilities.currentExtent;
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
    swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchainCreateInfo.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &ctx->swapchain) != VK_SUCCESS) {
        angle::log("vk_proxy: Failed to create swapchain for hwnd %p", hwnd);
        return ctx;
    }

    // Get swapchain images
    uint32_t imageCount = 0;
    vkGetSwapchainImagesKHR(device, ctx->swapchain, &imageCount, nullptr);
    ctx->swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, ctx->swapchain, &imageCount, ctx->swapchainImages.data());

    // Create image views
    ctx->swapchainImageViews.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; i++) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = ctx->swapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_B8G8R8A8_SRGB;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &viewInfo, nullptr, &ctx->swapchainImageViews[i]) != VK_SUCCESS) {
            angle::log("vk_proxy: Failed to create image view %u", i);
            return nullptr;
        }
    }

    // Create synchronization objects
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    vkCreateSemaphore(device, &semaphoreInfo, nullptr, &ctx->imageAvailableSemaphore);
    vkCreateSemaphore(device, &semaphoreInfo, nullptr, &ctx->renderFinishedSemaphore);

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    vkCreateFence(device, &fenceInfo, nullptr, &ctx->inFlightFence);

    angle::log("vk_proxy: Vulkan context created successfully (hdc=%p, hwnd=%p, swapchain images=%u)",
               hdc, hwnd, imageCount);

    return ctx;
}

bool VulkanState::makeContextCurrent(VulkanContext* ctx) {
    // Store current context in thread-local storage
    currentSurface = ctx ? ctx->surface : VK_NULL_HANDLE;
    return true;
}

bool VulkanState::presentFrame(VulkanContext* ctx) {
    if (!ctx || ctx->swapchain == VK_NULL_HANDLE) {
        return false;
    }

    // Acquire next image
    uint32_t imageIndex;
    VkResult acquireResult = vkAcquireNextImageKHR(device, ctx->swapchain, UINT64_MAX,
                                                   ctx->imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
        // Swapchain needs to be recreated (e.g., window resized)
        angle::log("vk_proxy: Swapchain out of date, recreation needed");
        return true; // Let caller handle recreation
    } else if (acquireResult != VK_SUCCESS) {
        angle::log("vk_proxy: Failed to acquire next image: %d", acquireResult);
        return false;
    }

    // Simple present without rendering
    // In a full implementation, this would execute command buffers
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &ctx->imageAvailableSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &ctx->swapchain;
    presentInfo.pImageIndices = &imageIndex;

    VkResult presentResult = vkQueuePresentKHR(presentQueue, &presentInfo);

    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        angle::log("vk_proxy: Swapchain presentation suboptimal, may need recreation");
    }

    vkQueueWaitIdle(presentQueue);
    return true;
}

} // namespace vkproxy
