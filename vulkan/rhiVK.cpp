#include "rhiVK.h"
#include "VulkanMacro.h"
#include "VulkanShader.h"
#include "main/rhi.h"
#include "vulkan/vulkan.h"
#include "glfw/glfw3.h"
#include "glm/glm.hpp"
#include "glslang/Public/ShaderLang.h"
#include "glslang/Public/ResourceLimits.h"
#include "glslang/SPIRV/GlslangToSpv.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>
#include <cstring>
#include <fstream>
#include <unordered_map>

#ifndef NDEBUG
#define VK_VALIDATION_LAYER
#endif

std::unordered_map<int, RHIKeyCode> RHIVK::actionCodeMap = {
        {GLFW_RELEASE,            RELEASE,            },
        {GLFW_PRESS,              PRESS,              },
};

std::unordered_map<int, RHIKeyCode> RHIVK::keyCodeMap = {
        {GLFW_MOUSE_BUTTON_LEFT,  MOUSE_BUTTON_LEFT,  },
        {GLFW_MOUSE_BUTTON_RIGHT, MOUSE_BUTTON_RIGHT, },
        {GLFW_MOUSE_BUTTON_MIDDLE,MOUSE_BUTTON_MIDDLE,},
        {GLFW_KEY_ESCAPE,         KEY_ESCAPE,         },
        {GLFW_KEY_0,              KEY_0,              },
        {GLFW_KEY_1,              KEY_1,              },
        {GLFW_KEY_2,              KEY_2,              },
        {GLFW_KEY_3,              KEY_3,              },
        {GLFW_KEY_4,              KEY_4,              },
        {GLFW_KEY_5,              KEY_5,              },
        {GLFW_KEY_6,              KEY_6,              },
        {GLFW_KEY_7,              KEY_7,              },
        {GLFW_KEY_8,              KEY_8,              },
        {GLFW_KEY_9,              KEY_9,              },
        {GLFW_KEY_A,              KEY_A,              },
        {GLFW_KEY_B,              KEY_B,              },
        {GLFW_KEY_C,              KEY_C,              },
        {GLFW_KEY_D,              KEY_D,              },
        {GLFW_KEY_E,              KEY_E,              },
        {GLFW_KEY_F,              KEY_F,              },
        {GLFW_KEY_G,              KEY_G,              },
        {GLFW_KEY_H,              KEY_H,              },
        {GLFW_KEY_I,              KEY_I,              },
        {GLFW_KEY_J,              KEY_J,              },
        {GLFW_KEY_K,              KEY_K,              },
        {GLFW_KEY_L,              KEY_L,              },
        {GLFW_KEY_M,              KEY_M,              },
        {GLFW_KEY_N,              KEY_N,              },
        {GLFW_KEY_O,              KEY_O,              },
        {GLFW_KEY_P,              KEY_P,              },
        {GLFW_KEY_Q,              KEY_Q,              },
        {GLFW_KEY_R,              KEY_R,              },
        {GLFW_KEY_S,              KEY_S,              },
        {GLFW_KEY_T,              KEY_T,              },
        {GLFW_KEY_U,              KEY_U,              },
        {GLFW_KEY_V,              KEY_V,              },
        {GLFW_KEY_W,              KEY_W,              },
        {GLFW_KEY_X,              KEY_X,              },
        {GLFW_KEY_Y,              KEY_Y,              },
        {GLFW_KEY_Z,              KEY_Z,              },
};

PFN_keyCallback RHIVK::keyCallback = nullptr;
PFN_mouseButtonCallback RHIVK::mouseButtonCallback = nullptr;
PFN_scrollCallback RHIVK::scrollCallback = nullptr;
PFN_cursorPosCallback RHIVK::cursorPosCallback = nullptr;

void RHIVK::glfwErrorCallback(int error, const char *description) {
    fputs(description, stderr);
}

void RHIVK::glfwKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if(RHIVK::keyCallback)
        RHIVK::keyCallback(keyCodeMap[key], actionCodeMap[action]);
}

void RHIVK::glfwMouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    if(RHIVK::mouseButtonCallback)
        RHIVK::mouseButtonCallback(keyCodeMap[button], actionCodeMap[action]);
}

void RHIVK::glfwScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    if(RHIVK::scrollCallback)
        RHIVK::scrollCallback(xoffset, yoffset);
}

void RHIVK::glfwCursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{
    if(RHIVK::cursorPosCallback)
        RHIVK::cursorPosCallback(xpos, ypos);
}

void RHIVK::init(int width, int height, bool vsync) {
    this->width = width;
    this->height = height;
    this->vsync = vsync;

    glfwSetErrorCallback(glfwErrorCallback);
    int glfwInitRes = glfwInit();
    assert(glfwInitRes == GLFW_TRUE);
    createInstance();
    initSurface();
    setPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createRenderPass();
    initPipeline();
    createFramebuffer();
    createCommandPoolAndBuffer();
    createSyncObjects();
}

void RHIVK::setCallback(PFN_cursorPosCallback newCursorPosCallback, PFN_scrollCallback newScrollCallback,
                        PFN_mouseButtonCallback newMouseButtonCallback, PFN_keyCallback newKeyCallback) {
    RHIVK::keyCallback = newKeyCallback;
    RHIVK::mouseButtonCallback = newMouseButtonCallback;
    RHIVK::scrollCallback = newScrollCallback;
    RHIVK::cursorPosCallback = newCursorPosCallback;

    glfwSetKeyCallback(window, RHIVK::glfwKeyCallback);
    glfwSetMouseButtonCallback(window, RHIVK::glfwMouseButtonCallback);
    glfwSetScrollCallback(window, RHIVK::glfwScrollCallback);
    glfwSetCursorPosCallback(window, RHIVK::glfwCursorPosCallback);
}

void RHIVK::pollEvents() {
    glfwPollEvents();
}

void *RHIVK::mapBuffer() {
    return nullptr;
}

void RHIVK::unmapBuffer() {

}

void RHIVK::draw(const char *title) {
    glfwSetWindowTitle(window, title);

    vkWaitForFences(device, 1, &commandBufferFinish, VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &commandBufferFinish);

    uint32_t frameBufferIndex;
    vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, framebufferReadyForRender, VK_NULL_HANDLE, &frameBufferIndex);

    vkResetCommandBuffer(commandBuffer, 0);
    generateCommandBuffer(frameBufferIndex);

    std::array<VkPipelineStageFlags, 1> waitStages{
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &framebufferReadyForRender;
    submitInfo.pWaitDstStageMask = waitStages.data();
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &framebufferReadyForPresent;

    VK_CHECK_RESULT(vkQueueSubmit(queue.graphics, 1, &submitInfo, commandBufferFinish));

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &framebufferReadyForPresent;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapChain;
    presentInfo.pImageIndices = &frameBufferIndex;
    presentInfo.pResults = nullptr;

    VK_CHECK_RESULT(vkQueuePresentKHR(queue.graphics, &presentInfo));
}

void RHIVK::destroy() {
    vkDeviceWaitIdle(device);

    vkDestroySemaphore(device, framebufferReadyForRender, nullptr);
    vkDestroySemaphore(device, framebufferReadyForPresent, nullptr);
    vkDestroyFence(device, commandBufferFinish, nullptr);

    vkDestroyCommandPool(device, commandPool, nullptr);

    for (auto& thisFramebuffer: framebuffers) {
        vkDestroyFramebuffer(device, thisFramebuffer, nullptr);
    }

    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

    vkDestroyRenderPass(device, renderPass, nullptr);

    for(auto& thisSwapChainImageView:swapChainImageViews)
    {
        vkDestroyImageView(device,thisSwapChainImageView, nullptr);
    }

    vkDestroySwapchainKHR(device, swapChain, nullptr);

    vkDestroyDevice(device, nullptr);

    vkDestroySurfaceKHR(instance, surface, nullptr);

    vkDestroyInstance(instance, nullptr);

    glfwDestroyWindow(window);
    glfwTerminate();
}

void RHIVK::createInstance() {
    instance = VK_NULL_HANDLE;
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "CUBIC Render";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "CUBIC Render";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = glfwExtensionCount;
    createInfo.ppEnabledExtensionNames = glfwExtensions;
    createInfo.enabledLayerCount = 0;

#ifdef VK_VALIDATION_LAYER
    enableValidationLayer(createInfo);
#endif

    VK_CHECK_RESULT(vkCreateInstance(&createInfo, nullptr, &instance));
    assert(instance!=VK_NULL_HANDLE);
}

void RHIVK::enableValidationLayer(VkInstanceCreateInfo &createInfo) {
    // Check if validation layer available
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    bool foundValidationLayer = false;
    for (const auto& layer : availableLayers)
    {
        if(strcmp(layer.layerName, VALIDATION_LAYER_NAME) == 0)
        {
            foundValidationLayer = true;
            break;
        }
    }

    if(!foundValidationLayer)
    {
        std::cout<<"[Warning] Vulkan validation layer is not supported.\n";
        return;
    }

    createInfo.enabledLayerCount = 1;
    createInfo.ppEnabledLayerNames = &VALIDATION_LAYER_NAME;
}

void RHIVK::initSurface() {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window = glfwCreateWindow(width, height, "", nullptr, nullptr);
    assert(window);

    VK_CHECK_RESULT(glfwCreateWindowSurface(instance, window, nullptr, &surface));
}

void RHIVK::setPhysicalDevice() {
    physicalDevice = VK_NULL_HANDLE;
    uint32_t physicalDeviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);
    assert(physicalDeviceCount > 0);
    std::vector<VkPhysicalDevice> availablePhysicalDevices(physicalDeviceCount);
    vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, availablePhysicalDevices.data());

    for(const auto& thisPhysicalDevice:availablePhysicalDevices)
    {
        if(checkDeviceSuitability(thisPhysicalDevice))
        {
            break;
        }
    }
    assert(physicalDevice!=VK_NULL_HANDLE);
}

bool RHIVK::checkDeviceSuitability(const VkPhysicalDevice &device) {
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceProperties(device, &properties);
    vkGetPhysicalDeviceFeatures(device, &features);

    // Check Device Properties and Features Here
    // Only Support NVIDIA CUDA GPU
    if(properties.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && properties.vendorID == NVIDIA_VENDOR_ID)
    {
        // Check Queue Family Here
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device,&queueFamilyCount,nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device,&queueFamilyCount,queueFamilyProperties.data());

        uint32_t index = 0;
        QueueFamily thisQueueFamily{};
        for(const auto& thisQueueFamilyProperty:queueFamilyProperties)
        {
            if(thisQueueFamilyProperty.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                thisQueueFamily.hasGraphics = true;
                thisQueueFamily.graphics = index;
            }

            // if(thisQueueFamilyProperty.queueFlags & VK_QUEUE_COMPUTE_BIT)
            // {
            //     thisQueueFamily.hasCompute = true;
            //     thisQueueFamily.compute = index;
            // }

            // if(thisQueueFamilyProperty.queueFlags & VK_QUEUE_TRANSFER_BIT)
            // {
            //     thisQueueFamily.hasTransfer = true;
            //     thisQueueFamily.transfer = index;
            // }

            index++;
        }

        // Only Support GPU With Graphics Queue Family
        if (thisQueueFamily.hasGraphics)
        {
            // Only Support Graphics Queue With Present Queue Support
            // Assume Device that Support Present Queue will also Support SwapChain
            VkBool32 presentSupport = false;
            VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceSupportKHR(device, thisQueueFamily.graphics, surface, &presentSupport));

            if(presentSupport)
            {
                // Simplified SwapChain Extension Checking
                std::cout << "[Log] Using " << properties.deviceName << "\n";
                physicalDevice = device;
                queueFamily = thisQueueFamily;
                return true;
            }
        }
    }
    return false;
}

void RHIVK::createLogicalDevice() {
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueFamily.graphics;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &HIGHEST_QUEUE_PRIORITY;

    VkPhysicalDeviceFeatures deviceFeatures{};
    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
    deviceCreateInfo.enabledExtensionCount = 1;
    deviceCreateInfo.ppEnabledExtensionNames = &SWAPCHAIN_EXTENSION_NAME;

    VK_CHECK_RESULT(vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device));

    vkGetDeviceQueue(device, queueFamily.graphics, 0, &queue.graphics);
}

void RHIVK::createSwapChain() {
    // Choose Surface Format
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
    assert(formatCount>0);
    std::vector<VkSurfaceFormatKHR> availableSurfaceFormats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, availableSurfaceFormats.data());

    for (const auto& thisSurfaceFormat:availableSurfaceFormats)
    {
        if(thisSurfaceFormat.format == VK_FORMAT_UNDEFINED)
        {
            swapChainFormat.format = VK_FORMAT_B8G8R8A8_SRGB;
            swapChainFormat.colorSpace = thisSurfaceFormat.colorSpace;
        }
        else if(thisSurfaceFormat.format == VK_FORMAT_B8G8R8A8_SRGB)
        {
            swapChainFormat = thisSurfaceFormat;
        }
        else if(thisSurfaceFormat.format == VK_FORMAT_B8G8R8A8_SRGB && thisSurfaceFormat.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR)
        {
            swapChainFormat = thisSurfaceFormat;
            break;
        }
    }
    assert(swapChainFormat.format == VK_FORMAT_B8G8R8A8_SRGB);

    // Choose Present Mode
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    if(!vsync)
    {
        uint32_t modeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &modeCount, nullptr);
        assert(modeCount>0);
        std::vector<VkPresentModeKHR> availablePresentMode(modeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &modeCount, availablePresentMode.data());
        for(const auto& thisPresentMode: availablePresentMode)
        {
            if(thisPresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR)
            {
                presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
                break;
            }
        }
    }

    // Choose Swap Extent
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);
    if (surfaceCapabilities.currentExtent.width != 0xffffffff)
    {
        swapChainExtent = surfaceCapabilities.currentExtent;
    }
    else
    {
        uint32_t framebufferWidth, framebufferHeight;
        glfwGetFramebufferSize(window, reinterpret_cast<int*>(&framebufferWidth), reinterpret_cast<int*>(&framebufferHeight));
        swapChainExtent = {glm::clamp(framebufferWidth, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width),
                           glm::clamp(framebufferHeight, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height)};
    }

    // Create Swap Chain
    uint32_t imageCount = glm::clamp(surfaceCapabilities.minImageCount + 1,
                                     surfaceCapabilities.minImageCount,
                                     surfaceCapabilities.maxImageCount>0?surfaceCapabilities.maxImageCount:0xffffffff);

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.minImageCount = imageCount;
    createInfo.surface = surface;
    createInfo.imageFormat = swapChainFormat.format;
    createInfo.imageColorSpace = swapChainFormat.colorSpace;
    createInfo.presentMode = presentMode;
    createInfo.imageExtent = swapChainExtent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;
    createInfo.pQueueFamilyIndices = nullptr;
    createInfo.preTransform = surfaceCapabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VK_CHECK_RESULT(vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain));

    uint32_t swapChainImageCount = 0;
    vkGetSwapchainImagesKHR(device, swapChain, &swapChainImageCount, nullptr);
    assert(swapChainImageCount>0);
    swapChainImages.resize(swapChainImageCount);
    vkGetSwapchainImagesKHR(device, swapChain, &swapChainImageCount, swapChainImages.data());
    swapChainImageViews.resize(swapChainImageCount);
    for(uint32_t i=0; i<imageCount; i++)
    {
        VkImageViewCreateInfo imageViewCreateInfo{};
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.image = swapChainImages[i];
        imageViewCreateInfo.format = swapChainFormat.format;
        imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.subresourceRange.layerCount = 1;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.levelCount = 1;
        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        VK_CHECK_RESULT(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &swapChainImageViews[i]));
    }
}

VkShaderModule RHIVK::createShaderModule(const VkShaderStageFlagBits shaderStage, const char *shaderCode) {
    glslang::InitializeProcess();

    EShLanguage shaderType;
    switch (shaderStage) {
        case VK_SHADER_STAGE_VERTEX_BIT:             shaderType = EShLangVertex;         break;
        case VK_SHADER_STAGE_FRAGMENT_BIT:           shaderType = EShLangFragment;       break;
        default: abort();
    }

    glslang::TShader shader(shaderType);
    shader.setStrings(&shaderCode, 1);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);

    if (!shader.parse(GetDefaultResources(), 460, false, EShMsgVulkanRules))
    {
        std::cout << "[Error] GLSL Parsing Failed!\n";
        std::cout << shader.getInfoLog() << "\n";
        std::cout << shader.getInfoDebugLog() << "\n";
        abort();
    }

    glslang::TProgram program;
    program.addShader(&shader);

    if (!program.link(EShMsgVulkanRules))
    {
        std::cout << "[Error] Program Linking Failed!\n";
        std::cout << program.getInfoLog() << "\n";
        std::cout << program.getInfoDebugLog() << "\n";
        abort();
    }

    std::vector<unsigned int> spirvCode;
    glslang::GlslangToSpv(*program.getIntermediate(shaderType), spirvCode);

    VkShaderModuleCreateInfo shaderModuleCreateInfo{};
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.codeSize = spirvCode.size() * sizeof(unsigned int);
    shaderModuleCreateInfo.pCode = spirvCode.data();

    VkShaderModule shaderModule;
    VK_CHECK_RESULT(vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &shaderModule));

    // Remember to finalize the process
    glslang::FinalizeProcess();

    return shaderModule;
}

void RHIVK::createRenderPass() {
    VkAttachmentDescription attachmentDescription{};
    attachmentDescription.format = swapChainFormat.format;
    attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    VkAttachmentReference attachmentReference{};
    attachmentReference.attachment = 0;
    attachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpassDescription{};
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &attachmentReference;
    subpassDescription.inputAttachmentCount = 0;
    subpassDescription.pInputAttachments = nullptr;
    subpassDescription.pDepthStencilAttachment = nullptr;
    subpassDescription.preserveAttachmentCount = 0;
    subpassDescription.pPreserveAttachments = nullptr;
    subpassDescription.pResolveAttachments = nullptr;
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    VkSubpassDependency inputSubpassDependency{};
    inputSubpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    inputSubpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    inputSubpassDependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    inputSubpassDependency.dstSubpass = 0;
    inputSubpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    inputSubpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

    VkSubpassDependency outputSubpassDependency{};
    outputSubpassDependency.srcSubpass = 0;
    outputSubpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    outputSubpassDependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    outputSubpassDependency.dstSubpass = VK_SUBPASS_EXTERNAL;
    outputSubpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    outputSubpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

    std::array<VkSubpassDependency, 2> subpassDependency{
            inputSubpassDependency,
            outputSubpassDependency
    };


    VkRenderPassCreateInfo renderPassCreateInfo{};
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.attachmentCount = 1;
    renderPassCreateInfo.pAttachments = &attachmentDescription;
    renderPassCreateInfo.dependencyCount = subpassDependency.size();
    renderPassCreateInfo.pDependencies = subpassDependency.data();
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpassDescription;

    VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderPass));
}

void RHIVK::initPipeline() {
    // [Temporal Disabled] Dynamic State
        std::vector<VkDynamicState> dynamicStates = {
                // VK_DYNAMIC_STATE_SCISSOR,
                // VK_DYNAMIC_STATE_VIEWPORT
        };
        VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo{};
        dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicStateCreateInfo.dynamicStateCount = dynamicStates.size();
        dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

    // Vertex Input
    VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo{};
    vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputStateCreateInfo.vertexBindingDescriptionCount = 0;
    vertexInputStateCreateInfo.pVertexBindingDescriptions = nullptr;
    vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 0;
    vertexInputStateCreateInfo.pVertexAttributeDescriptions = nullptr;

    // Input Assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo{};
    inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

    // Viewport and Scissor
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = swapChainExtent.width;
    viewport.height = swapChainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.extent = swapChainExtent;
    scissor.offset = {0, 0};

    VkPipelineViewportStateCreateInfo viewportStateCreateInfo{};
    viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportStateCreateInfo.scissorCount = 1;
    viewportStateCreateInfo.pScissors = &scissor;
    viewportStateCreateInfo.viewportCount = 1;
    viewportStateCreateInfo.pViewports = &viewport;

    // Rasterization
    VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{};
    rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
    rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationStateCreateInfo.lineWidth = 1.0f;
    rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
    rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
    rasterizationStateCreateInfo.depthBiasClamp = 0.0f;
    rasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
    rasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo{};
    multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
    multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampleStateCreateInfo.minSampleShading = 1.0f;
    multisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
    multisampleStateCreateInfo.alphaToOneEnable = VK_FALSE;
    multisampleStateCreateInfo.pSampleMask = nullptr;

    // Depth and Stencil
    VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo{};
    depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilStateCreateInfo.depthTestEnable = VK_FALSE;
    depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
    depthStencilStateCreateInfo.depthWriteEnable = VK_FALSE;
    depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencilStateCreateInfo.minDepthBounds = 0.0f;
    depthStencilStateCreateInfo.maxDepthBounds = 1.0f;
    depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
    depthStencilStateCreateInfo.front.depthFailOp = VK_STENCIL_OP_KEEP;
    depthStencilStateCreateInfo.front.failOp = VK_STENCIL_OP_KEEP;
    depthStencilStateCreateInfo.front.passOp = VK_STENCIL_OP_KEEP;
    depthStencilStateCreateInfo.front.compareOp = VK_COMPARE_OP_ALWAYS;
    depthStencilStateCreateInfo.front.compareMask = 0;
    depthStencilStateCreateInfo.front.reference = 0;
    depthStencilStateCreateInfo.front.writeMask = 0;
    depthStencilStateCreateInfo.back = depthStencilStateCreateInfo.front;

    // Color Blend
    VkPipelineColorBlendAttachmentState colorBlendAttachmentState{};
    colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                               VK_COLOR_COMPONENT_G_BIT |
                                               VK_COLOR_COMPONENT_B_BIT |
                                               VK_COLOR_COMPONENT_A_BIT ;
    colorBlendAttachmentState.blendEnable = VK_FALSE;
    colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo{};
    colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
    colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
    colorBlendStateCreateInfo.attachmentCount = 1;
    colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;
    colorBlendStateCreateInfo.blendConstants[0] = 0.0f;
    colorBlendStateCreateInfo.blendConstants[1] = 0.0f;
    colorBlendStateCreateInfo.blendConstants[2] = 0.0f;
    colorBlendStateCreateInfo.blendConstants[3] = 0.0f;

    // Pipeline Layout
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = 0;
    pipelineLayoutCreateInfo.pSetLayouts = nullptr;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

    VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

    // Shader Stage
    VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo{};
    vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexShaderStageCreateInfo.module = createShaderModule(VK_SHADER_STAGE_VERTEX_BIT, defaultShader::vertex);
    vertexShaderStageCreateInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo{};
    fragmentShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragmentShaderStageCreateInfo.module = createShaderModule(VK_SHADER_STAGE_FRAGMENT_BIT, defaultShader::fragment);
    fragmentShaderStageCreateInfo.pName = "main";

    std::array<VkPipelineShaderStageCreateInfo,2> shaderStageCreateInfo{
            vertexShaderStageCreateInfo,
            fragmentShaderStageCreateInfo
    };

    VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.renderPass = renderPass;
    pipelineCreateInfo.subpass = 0;
    pipelineCreateInfo.layout = pipelineLayout;
    pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineCreateInfo.basePipelineIndex = -1;
    pipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
    pipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
    pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
    pipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
    pipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
    pipelineCreateInfo.pTessellationState = nullptr;
    pipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
    pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
    pipelineCreateInfo.stageCount = shaderStageCreateInfo.size();
    pipelineCreateInfo.pStages = shaderStageCreateInfo.data();

    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline));

    for (auto& shaderStage: shaderStageCreateInfo)
    {
        vkDestroyShaderModule(device, shaderStage.module, nullptr);
    }

}

void RHIVK::createFramebuffer() {
    framebuffers.resize(swapChainImageViews.size());
    auto framebufferIter = framebuffers.begin();
    for (auto& thisSwapChainImageView: swapChainImageViews)
    {
        VkFramebufferCreateInfo framebufferCreateInfo{};
        framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferCreateInfo.renderPass = renderPass;
        framebufferCreateInfo.attachmentCount = 1;
        framebufferCreateInfo.pAttachments = &thisSwapChainImageView;
        framebufferCreateInfo.height = swapChainExtent.height;
        framebufferCreateInfo.width = swapChainExtent.width;
        framebufferCreateInfo.layers = 1;

        VK_CHECK_RESULT(vkCreateFramebuffer(device, &framebufferCreateInfo, nullptr, &(*framebufferIter)));

        framebufferIter++;
    }
}

void RHIVK::createCommandPoolAndBuffer() {
    VkCommandPoolCreateInfo commandPoolCreateInfo{};
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolCreateInfo.queueFamilyIndex = queueFamily.graphics;

    VK_CHECK_RESULT(vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &commandPool));

    VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = commandPool;
    commandBufferAllocateInfo.commandBufferCount = 1;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer));
}

void RHIVK::createSyncObjects() {
    VkSemaphoreCreateInfo semaphoreCreateInfo{};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &framebufferReadyForRender));
    VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &framebufferReadyForPresent));

    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    VK_CHECK_RESULT(vkCreateFence(device, &fenceCreateInfo, nullptr, &commandBufferFinish));
}

void RHIVK::generateCommandBuffer(const uint32_t framebufferIndex) {
    VkCommandBufferBeginInfo commandBufferBeginInfo{};
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    commandBufferBeginInfo.flags = 0;
    commandBufferBeginInfo.pInheritanceInfo = nullptr;

    VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));

    VkClearValue clearValue{};
    clearValue.color = {0,0,0,0};

    VkRenderPassBeginInfo renderPassBeginInfo{};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.renderPass = renderPass;
    renderPassBeginInfo.framebuffer = framebuffers[framebufferIndex];
    renderPassBeginInfo.clearValueCount = 1;
    renderPassBeginInfo.pClearValues = &clearValue;
    renderPassBeginInfo.renderArea.offset = {0,0};
    renderPassBeginInfo.renderArea.extent = swapChainExtent;

    vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(commandBuffer);

    VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));
}
