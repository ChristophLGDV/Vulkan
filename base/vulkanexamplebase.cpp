/*
* Vulkan Example base class
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "vulkanexamplebase.h"

VkResult VulkanExampleBase::createInstance(bool enableValidation) {
    this->enableValidation = enableValidation;

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = name.c_str();
    appInfo.pEngineName = name.c_str();
    appInfo.apiVersion = VK_API_VERSION_1_0;

    std::vector<const char*> enabledExtensions = { VK_KHR_SURFACE_EXTENSION_NAME };

    // Enable surface extensions depending on os
#if defined(_WIN32)
    enabledExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(__ANDROID__)
    enabledExtensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#elif defined(__linux__)
    enabledExtensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#endif

    VkInstanceCreateInfo instanceCreateInfo = {};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pNext = NULL;
    instanceCreateInfo.pApplicationInfo = &appInfo;
    if (enabledExtensions.size() > 0) {
        if (enableValidation) {
            enabledExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
        }
        instanceCreateInfo.enabledExtensionCount = (uint32_t)enabledExtensions.size();
        instanceCreateInfo.ppEnabledExtensionNames = enabledExtensions.data();
    }
    if (enableValidation) {
        instanceCreateInfo.enabledLayerCount = vkDebug::validationLayerCount;
        instanceCreateInfo.ppEnabledLayerNames = vkDebug::validationLayerNames;
    }
    return vkCreateInstance(&instanceCreateInfo, nullptr, &instance);
}

VkResult VulkanExampleBase::createDevice(VkDeviceQueueCreateInfo requestedQueues, bool enableValidation) {
    std::vector<const char*> enabledExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = NULL;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &requestedQueues;
    deviceCreateInfo.pEnabledFeatures = NULL;

    // enable the debug marker extension if it is present (likely meaning a debugging tool is present)
    if (vkTools::checkDeviceExtensionPresent(physicalDevice, VK_EXT_DEBUG_MARKER_EXTENSION_NAME)) {
        enabledExtensions.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
        enableDebugMarkers = true;
    }

    if (enabledExtensions.size() > 0) {
        deviceCreateInfo.enabledExtensionCount = (uint32_t)enabledExtensions.size();
        deviceCreateInfo.ppEnabledExtensionNames = enabledExtensions.data();
    }
    if (enableValidation) {
        deviceCreateInfo.enabledLayerCount = vkDebug::validationLayerCount;
        deviceCreateInfo.ppEnabledLayerNames = vkDebug::validationLayerNames;
    }

    return vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device);
}

std::string VulkanExampleBase::getWindowTitle() {
    std::string device(deviceProperties.deviceName);
    std::string windowTitle;
    windowTitle = title + " - " + device + " - " + std::to_string(frameCounter) + " fps";
    return windowTitle;
}

const std::string VulkanExampleBase::getAssetPath() {
#if defined(__ANDROID__)
    return "";
#else
    return "./../data/";
#endif
}

bool VulkanExampleBase::checkCommandBuffers() {
    for (auto& cmdBuffer : drawCmdBuffers) {
        if (cmdBuffer == VK_NULL_HANDLE) {
            return false;
        }
    }
    return true;
}

void VulkanExampleBase::createCommandBuffers() {
    // Create one command buffer per frame buffer
    // in the swap chain
    // Command buffers store a reference to the
    // frame buffer inside their render pass info
    // so for static usage withouth having to rebuild
    // them each frame, we use one per frame buffer

    drawCmdBuffers.resize(swapChain.imageCount);
    prePresentCmdBuffers.resize(swapChain.imageCount);
    postPresentCmdBuffers.resize(swapChain.imageCount);

    VkCommandBufferAllocateInfo cmdBufAllocateInfo =
        vkTools::initializers::commandBufferAllocateInfo(
            cmdPool,
            VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            static_cast<uint32_t>(drawCmdBuffers.size()));

    VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, drawCmdBuffers.data()));

    // Command buffers for submitting present barriers
    // One pre and post present buffer per swap chain image
    VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, prePresentCmdBuffers.data()));
    VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, postPresentCmdBuffers.data()));
}

void VulkanExampleBase::destroyCommandBuffers() {
    vkFreeCommandBuffers(device, cmdPool, static_cast<uint32_t>(drawCmdBuffers.size()), drawCmdBuffers.data());
    vkFreeCommandBuffers(device, cmdPool, static_cast<uint32_t>(drawCmdBuffers.size()), prePresentCmdBuffers.data());
    vkFreeCommandBuffers(device, cmdPool, static_cast<uint32_t>(drawCmdBuffers.size()), postPresentCmdBuffers.data());
}

void VulkanExampleBase::createSetupCommandBuffer() {
    if (setupCmdBuffer != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(device, cmdPool, 1, &setupCmdBuffer);
        setupCmdBuffer = VK_NULL_HANDLE; // todo : check if still necessary
    }

    VkCommandBufferAllocateInfo cmdBufAllocateInfo =
        vkTools::initializers::commandBufferAllocateInfo(
            cmdPool,
            VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            1);

    VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &setupCmdBuffer));

    VkCommandBufferBeginInfo cmdBufInfo = {};
    cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VK_CHECK_RESULT(vkBeginCommandBuffer(setupCmdBuffer, &cmdBufInfo));
}

void VulkanExampleBase::flushSetupCommandBuffer() {
    if (setupCmdBuffer == VK_NULL_HANDLE)
        return;

    VK_CHECK_RESULT(vkEndCommandBuffer(setupCmdBuffer));

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &setupCmdBuffer;

    VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
    VK_CHECK_RESULT(vkQueueWaitIdle(queue));

    vkFreeCommandBuffers(device, cmdPool, 1, &setupCmdBuffer);
    setupCmdBuffer = VK_NULL_HANDLE;
}

VkCommandBuffer VulkanExampleBase::createCommandBuffer(VkCommandBufferLevel level, bool begin) {
    VkCommandBuffer cmdBuffer;

    VkCommandBufferAllocateInfo cmdBufAllocateInfo =
        vkTools::initializers::commandBufferAllocateInfo(
            cmdPool,
            level,
            1);

    VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &cmdBuffer));

    // If requested, also start the new command buffer
    if (begin) {
        VkCommandBufferBeginInfo cmdBufInfo = vkTools::initializers::commandBufferBeginInfo();
        VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));
    }

    return cmdBuffer;
}

void VulkanExampleBase::flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free) {
    if (commandBuffer == VK_NULL_HANDLE) {
        return;
    }

    VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
    VK_CHECK_RESULT(vkQueueWaitIdle(queue));

    if (free) {
        vkFreeCommandBuffers(device, cmdPool, 1, &commandBuffer);
    }
}

void VulkanExampleBase::createPipelineCache() {
    VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
    pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    VK_CHECK_RESULT(vkCreatePipelineCache(device, &pipelineCacheCreateInfo, nullptr, &pipelineCache));
}

void VulkanExampleBase::prepare() {
    if (enableValidation) {
        // The report flags determine what type of messages for the layers will be displayed
        // For validating (debugging) an appplication the error and warning bits should suffice
        VkDebugReportFlagsEXT debugReportFlags = VK_DEBUG_REPORT_ERROR_BIT_EXT; // VK_DEBUG_REPORT_WARNING_BIT_EXT (enable to also display warnings)
        // Additional flags include performance info, loader and layer debug messages, etc.
        vkDebug::setupDebugging(instance, debugReportFlags, VK_NULL_HANDLE);
    }
    if (enableDebugMarkers) {
        vkDebug::DebugMarker::setup(device);
    }
    createCommandPool();
    createSetupCommandBuffer();
    setupSwapChain();
    createCommandBuffers();
    buildPresentCommandBuffers();
    setupDepthStencil();
    setupRenderPass();
    createPipelineCache();
    setupFrameBuffer();
    flushSetupCommandBuffer();
    // Recreate setup command buffer for derived class
    createSetupCommandBuffer();
    // Create a simple texture loader class
    textureLoader = new vkTools::VulkanTextureLoader(physicalDevice, device, queue, cmdPool);
#if defined(__ANDROID__)
    textureLoader->assetManager = androidApp->activity->assetManager;
#endif
    if (enableTextOverlay) {
        // Load the text rendering shaders
        std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
        shaderStages.push_back(loadShader(getAssetPath() + "shaders/base/textoverlay.vert.spv", VK_SHADER_STAGE_VERTEX_BIT));
        shaderStages.push_back(loadShader(getAssetPath() + "shaders/base/textoverlay.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT));
        textOverlay = new VulkanTextOverlay(
            physicalDevice,
            device,
            queue,
            frameBuffers,
            colorformat,
            depthFormat,
            &width,
            &height,
            shaderStages
            );
        updateTextOverlay();
    }
}

VkPipelineShaderStageCreateInfo VulkanExampleBase::loadShader(std::string fileName, VkShaderStageFlagBits stage) {
    VkPipelineShaderStageCreateInfo shaderStage = {};
    shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStage.stage = stage;
#if defined(__ANDROID__)
    shaderStage.module = vkTools::loadShader(androidApp->activity->assetManager, fileName.c_str(), device, stage);
#else
    shaderStage.module = vkTools::loadShader(fileName.c_str(), device, stage);
#endif
    shaderStage.pName = "main"; // todo : make param
    assert(shaderStage.module != NULL);
    shaderModules.push_back(shaderStage.module);
    return shaderStage;
}

VkBool32 VulkanExampleBase::createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size, void * data, VkBuffer * buffer, VkDeviceMemory * memory) {
    VkMemoryRequirements memReqs;
    VkMemoryAllocateInfo memAlloc = vkTools::initializers::memoryAllocateInfo();
    VkBufferCreateInfo bufferCreateInfo = vkTools::initializers::bufferCreateInfo(usageFlags, size);

    VK_CHECK_RESULT(vkCreateBuffer(device, &bufferCreateInfo, nullptr, buffer));

    vkGetBufferMemoryRequirements(device, *buffer, &memReqs);
    memAlloc.allocationSize = memReqs.size;
    memAlloc.memoryTypeIndex = getMemoryType(memReqs.memoryTypeBits, memoryPropertyFlags);
    VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, memory));
    if (data != nullptr) {
        void *mapped;
        VK_CHECK_RESULT(vkMapMemory(device, *memory, 0, size, 0, &mapped));
        memcpy(mapped, data, size);
        vkUnmapMemory(device, *memory);
    }
    VK_CHECK_RESULT(vkBindBufferMemory(device, *buffer, *memory, 0));

    return true;
}

VkBool32 VulkanExampleBase::createBuffer(VkBufferUsageFlags usage, VkDeviceSize size, void * data, VkBuffer *buffer, VkDeviceMemory *memory) {
    return createBuffer(usage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, size, data, buffer, memory);
}

VkBool32 VulkanExampleBase::createBuffer(VkBufferUsageFlags usage, VkDeviceSize size, void * data, VkBuffer * buffer, VkDeviceMemory * memory, VkDescriptorBufferInfo * descriptor) {
    VkBool32 res = createBuffer(usage, size, data, buffer, memory);
    if (res) {
        descriptor->offset = 0;
        descriptor->buffer = *buffer;
        descriptor->range = size;
        return true;
    } else {
        return false;
    }
}

VkBool32 VulkanExampleBase::createBuffer(VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size, void * data, VkBuffer * buffer, VkDeviceMemory * memory, VkDescriptorBufferInfo * descriptor) {
    VkBool32 res = createBuffer(usage, memoryPropertyFlags, size, data, buffer, memory);
    if (res) {
        descriptor->offset = 0;
        descriptor->buffer = *buffer;
        descriptor->range = size;
        return true;
    } else {
        return false;
    }
}

void VulkanExampleBase::loadMesh(
    std::string filename,
    vkMeshLoader::MeshBuffer * meshBuffer,
    std::vector<vkMeshLoader::VertexLayout> vertexLayout,
    float scale) {
    VulkanMeshLoader *mesh = new VulkanMeshLoader();

#if defined(__ANDROID__)
    mesh->assetManager = androidApp->activity->assetManager;
#endif

    mesh->LoadMesh(filename);
    assert(mesh->m_Entries.size() > 0);

    VkCommandBuffer copyCmd = VulkanExampleBase::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);

    mesh->createBuffers(
        device,
        deviceMemoryProperties,
        meshBuffer,
        vertexLayout,
        scale,
        true,
        copyCmd,
        queue);

    vkFreeCommandBuffers(device, cmdPool, 1, &copyCmd);

    meshBuffer->dim = mesh->dim.size;

    delete(mesh);
}

void VulkanExampleBase::update(float delta) {
    bool updateView = false;
    frameCounter++;
    frameTimer = delta;
    camera.update(frameTimer);
    if (camera.moving()) {
        updateView = true;
    }
    // Convert to clamped timer value
    if (!paused) {
        timer += timerSpeed * frameTimer;
        if (timer > 1.0) {
            timer -= 1.0f;
        }
    }
    fpsTimer += (float)delta;
    if (fpsTimer > 1.0f) {
        std::string windowTitle = getWindowTitle();
        lastFPS = frameCounter;
        updateTextOverlay();
        fpsTimer = 0.0f;
        frameCounter = 0;
#if !defined(__ANDROID__)
        if (!enableTextOverlay) {
            glfwSetWindowTitle(window, windowTitle.c_str());
        }
#endif
    }

#if defined(__ANDROID__)
    // Check gamepad state
    const float deadZone = 0.0015f;
    // todo : check if gamepad is present
    // todo : time based and relative axis positions
    if (camera.type != Camera::CameraType::firstperson) {
        // Rotate
        if (std::abs(gamePadState.axisLeft.x) > deadZone) {
            rotation.y += gamePadState.axisLeft.x * 0.5f * rotationSpeed;
            camera.rotate(glm::vec3(0.0f, gamePadState.axisLeft.x * 0.5f, 0.0f));
            updateView = true;
        }
        if (std::abs(gamePadState.axisLeft.y) > deadZone) {
            rotation.x -= gamePadState.axisLeft.y * 0.5f * rotationSpeed;
            camera.rotate(glm::vec3(gamePadState.axisLeft.y * 0.5f, 0.0f, 0.0f));
            updateView = true;
        }
        // Zoom
        if (std::abs(gamePadState.axisRight.y) > deadZone) {
            zoom -= gamePadState.axisRight.y * 0.01f * zoomSpeed;
            updateView = true;
        }
    } else {
        updateView = camera.updatePad(gamePadState.axisLeft, gamePadState.axisRight, frameTimer);
    }
#endif
    if (updateView) {
        viewChanged();
    }
}

void VulkanExampleBase::renderLoop() {
#if defined(__ANDROID__)
    while (1) {
        int ident;
        int events;
        struct android_poll_source* source;
        bool destroy = false;

        focused = true;

        while ((ident = ALooper_pollAll(focused ? 0 : -1, NULL, &events, (void**)&source)) >= 0) {
            if (source != NULL) {
                source->process(androidApp, source);
            }
            if (androidApp->destroyRequested != 0) {
                LOGD("Android app destroy requested");
                destroy = true;
                break;
            }
        }

        // App destruction requested
        // Exit loop, example will be destroyed in application main
        if (destroy) {
            break;
        }

        // Render frame
        if (prepared) {
            auto tStart = std::chrono::high_resolution_clock::now();
            render();
            auto tEnd = std::chrono::high_resolution_clock::now();
            auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
            float tDiffSeconds = tDiff / 1000.0f;
            update(tDiffSeconds);
        }
    }
#else 
    auto tStart = std::chrono::high_resolution_clock::now();
    while (!glfwWindowShouldClose(window)) {
        auto tEnd = std::chrono::high_resolution_clock::now();
        auto tDiff = std::chrono::duration<float, std::milli>(tEnd - tStart).count();
        auto tDiffSeconds = tDiff / 1000.0f;
        tStart = tEnd;
        glfwPollEvents();
        render();
        update(tDiffSeconds);
    }
#endif
    // Flush device to make sure all resources can be freed 
    vkQueueWaitIdle(queue);
    vkDeviceWaitIdle(device);
}

VkSubmitInfo VulkanExampleBase::prepareSubmitInfo(
    std::vector<VkCommandBuffer> commandBuffers,
    VkPipelineStageFlags *pipelineStages) {
    VkSubmitInfo submitInfo = vkTools::initializers::submitInfo();
    submitInfo.pWaitDstStageMask = pipelineStages;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &semaphores.presentComplete;
    submitInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
    submitInfo.pCommandBuffers = commandBuffers.data();
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &semaphores.renderComplete;
    return submitInfo;
}

void VulkanExampleBase::updateTextOverlay() {
    if (!enableTextOverlay)
        return;

    textOverlay->beginTextUpdate();

    textOverlay->addText(title, 5.0f, 5.0f, VulkanTextOverlay::alignLeft);

    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << (frameTimer * 1000.0f) << "ms (" << lastFPS << " fps)";
    textOverlay->addText(ss.str(), 5.0f, 25.0f, VulkanTextOverlay::alignLeft);

    textOverlay->addText(deviceProperties.deviceName, 5.0f, 45.0f, VulkanTextOverlay::alignLeft);

    getOverlayText(textOverlay);

    textOverlay->endTextUpdate();
}

void VulkanExampleBase::getOverlayText(VulkanTextOverlay *textOverlay) {
    // Can be overriden in derived class
}

void VulkanExampleBase::prepareFrame() {
    // Acquire the next image from the swap chaing
    VK_CHECK_RESULT(swapChain.acquireNextImage(semaphores.presentComplete, &currentBuffer));
    /*
        // Insert a post present image barrier to transform the image back to a
        // color attachment that our render pass can write to
        // We always use undefined image layout as the source as it doesn't actually matter
        // what is done with the previous image contents
        VkCommandBufferBeginInfo cmdBufInfo = vkTools::initializers::commandBufferBeginInfo();
        VK_CHECK_RESULT(vkBeginCommandBuffer(postPresentCmdBuffer, &cmdBufInfo));

        VkImageMemoryBarrier postPresentBarrier = vkTools::initializers::imageMemoryBarrier();
        postPresentBarrier.srcAccessMask = 0;
        postPresentBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        postPresentBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        postPresentBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        postPresentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        postPresentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        postPresentBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        postPresentBarrier.image = swapChain.buffers[currentBuffer].image;

        vkCmdPipelineBarrier(
            postPresentCmdBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &postPresentBarrier);

        VK_CHECK_RESULT(vkEndCommandBuffer(postPresentCmdBuffer));
    */

    // Submit post present image barrier to transform the image back to a color attachment that our render pass can write to
    VkSubmitInfo submitInfo = vkTools::initializers::submitInfo();
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &postPresentCmdBuffers[currentBuffer];
    VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
}

void VulkanExampleBase::submitFrame() {
    bool submitTextOverlay = enableTextOverlay && textOverlay->visible;

    if (submitTextOverlay) {
        // Wait for color attachment output to finish before rendering the text overlay
        VkPipelineStageFlags stageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        submitInfo.pWaitDstStageMask = &stageFlags;

        // Set semaphores
        // Wait for render complete semaphore
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &semaphores.renderComplete;
        // Signal ready with text overlay complete semaphpre
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &semaphores.textOverlayComplete;

        // Submit current text overlay command buffer
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &textOverlay->cmdBuffers[currentBuffer];
        VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));

        // Reset stage mask
        submitInfo.pWaitDstStageMask = &submitPipelineStages;
        // Reset wait and signal semaphores for rendering next frame
        // Wait for swap chain presentation to finish
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &semaphores.presentComplete;
        // Signal ready with offscreen semaphore
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &semaphores.renderComplete;
    }

    // Submit a pre present image barrier to the queue
    // Transforms the (framebuffer) image layout from color attachment to present(khr) for presenting to the swap chain
    /*
    VkCommandBufferBeginInfo cmdBufInfo = vkTools::initializers::commandBufferBeginInfo();
    VK_CHECK_RESULT(vkBeginCommandBuffer(prePresentCmdBuffer, &cmdBufInfo));

    VkImageMemoryBarrier prePresentBarrier = vkTools::initializers::imageMemoryBarrier();
    prePresentBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    prePresentBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    prePresentBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    prePresentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    prePresentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    prePresentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    prePresentBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    prePresentBarrier.image = swapChain.buffers[currentBuffer].image;

    vkCmdPipelineBarrier(
        prePresentCmdBuffer,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        VK_FLAGS_NONE,
        0, nullptr, // No memory barriers,
        0, nullptr, // No buffer barriers,
        1, &prePresentBarrier);

    VK_CHECK_RESULT(vkEndCommandBuffer(prePresentCmdBuffer));
    */
    // Submit pre present image barrier to transform the image from color attachment to present(khr) for presenting to the swap chain
    VkSubmitInfo submitInfo = vkTools::initializers::submitInfo();
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &prePresentCmdBuffers[currentBuffer];
    VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));

    VK_CHECK_RESULT(swapChain.queuePresent(queue, currentBuffer, submitTextOverlay ? semaphores.textOverlayComplete : semaphores.renderComplete));

    VK_CHECK_RESULT(vkQueueWaitIdle(queue));
}

VulkanExampleBase::VulkanExampleBase(bool enableValidation) {
    // Check for validation command line flag
#if defined(_WIN32)
    for (int32_t i = 0; i < __argc; i++) {
        if (__argv[i] == std::string("-validation")) {
            enableValidation = true;
        }
        if (__argv[i] == std::string("-vsync")) {
            enableVSync = true;
        }
    }
#elif defined(__ANDROID__)
    // Vulkan library is loaded dynamically on Android
    bool libLoaded = loadVulkanLibrary();
    assert(libLoaded);
#elif defined(__linux__)
    initxcbConnection();
#endif

#if !defined(__ANDROID__)
    // Android Vulkan initialization is handled in APP_CMD_INIT_WINDOW event
    initVulkan(enableValidation);
#endif

#if defined(_WIN32)
    // Enable console if validation is active
    // Debug message callback will output to it
    if (enableValidation) {
        setupConsole("VulkanExample");
    }
#endif
}

VulkanExampleBase::~VulkanExampleBase() {
    // Clean up Vulkan resources
    swapChain.cleanup();
    if (descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    }
    if (setupCmdBuffer != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(device, cmdPool, 1, &setupCmdBuffer);

    }
    destroyCommandBuffers();
    vkDestroyRenderPass(device, renderPass, nullptr);
    for (uint32_t i = 0; i < frameBuffers.size(); i++) {
        vkDestroyFramebuffer(device, frameBuffers[i], nullptr);
    }

    for (auto& shaderModule : shaderModules) {
        vkDestroyShaderModule(device, shaderModule, nullptr);
    }
    vkDestroyImageView(device, depthStencil.view, nullptr);
    vkDestroyImage(device, depthStencil.image, nullptr);
    vkFreeMemory(device, depthStencil.mem, nullptr);

    vkDestroyPipelineCache(device, pipelineCache, nullptr);

    if (textureLoader) {
        delete textureLoader;
    }

    vkDestroyCommandPool(device, cmdPool, nullptr);

    vkDestroySemaphore(device, semaphores.presentComplete, nullptr);
    vkDestroySemaphore(device, semaphores.renderComplete, nullptr);
    vkDestroySemaphore(device, semaphores.textOverlayComplete, nullptr);

    if (enableTextOverlay) {
        delete textOverlay;
    }

    vkDestroyDevice(device, nullptr);

    if (enableValidation) {
        vkDebug::freeDebugCallback(instance);
    }

    vkDestroyInstance(instance, nullptr);

#if defined(__linux)
#if defined(__ANDROID__)
    // todo : android cleanup (if required)
#else
    xcb_destroy_window(connection, window);
    xcb_disconnect(connection);
#endif
#endif
}

void VulkanExampleBase::initVulkan(bool enableValidation) {
    VkResult err;

    // Vulkan instance
    err = createInstance(enableValidation);
    if (err) {
        vkTools::exitFatal("Could not create Vulkan instance : \n" + vkTools::errorString(err), "Fatal error");
    }

#if defined(__ANDROID__)
    loadVulkanFunctions(instance);
#endif

    // Physical device
    uint32_t gpuCount = 0;
    // Get number of available physical devices
    VK_CHECK_RESULT(vkEnumeratePhysicalDevices(instance, &gpuCount, nullptr));
    assert(gpuCount > 0);
    // Enumerate devices
    std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
    err = vkEnumeratePhysicalDevices(instance, &gpuCount, physicalDevices.data());
    if (err) {
        vkTools::exitFatal("Could not enumerate phyiscal devices : \n" + vkTools::errorString(err), "Fatal error");
    }

    // Note :
    // This example will always use the first physical device reported,
    // change the vector index if you have multiple Vulkan devices installed
    // and want to use another one
    physicalDevice = physicalDevices[0];

    // Find a queue that supports graphics operations
    uint32_t graphicsQueueIndex = 0;
    uint32_t queueCount;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, NULL);
    assert(queueCount >= 1);

    std::vector<VkQueueFamilyProperties> queueProps;
    queueProps.resize(queueCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, queueProps.data());

    for (graphicsQueueIndex = 0; graphicsQueueIndex < queueCount; graphicsQueueIndex++) {
        if (queueProps[graphicsQueueIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            break;
    }
    assert(graphicsQueueIndex < queueCount);

    // Vulkan device
    std::array<float, 1> queuePriorities = { 0.0f };
    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = graphicsQueueIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = queuePriorities.data();

    VK_CHECK_RESULT(createDevice(queueCreateInfo, enableValidation));

    // Store properties (including limits) and features of the phyiscal device
    // So examples can check against them and see if a feature is actually supported
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
    vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);

    // Gather physical device memory properties
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemoryProperties);

    // Get the graphics queue
    vkGetDeviceQueue(device, graphicsQueueIndex, 0, &queue);

    // Find a suitable depth format
    VkBool32 validDepthFormat = vkTools::getSupportedDepthFormat(physicalDevice, &depthFormat);
    assert(validDepthFormat);

    swapChain.connect(instance, physicalDevice, device);

    // Create synchronization objects
    VkSemaphoreCreateInfo semaphoreCreateInfo = vkTools::initializers::semaphoreCreateInfo();
    // Create a semaphore used to synchronize image presentation
    // Ensures that the image is displayed before we start submitting new commands to the queu
    VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphores.presentComplete));
    // Create a semaphore used to synchronize command submission
    // Ensures that the image is not presented until all commands have been sumbitted and executed
    VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphores.renderComplete));
    // Create a semaphore used to synchronize command submission
    // Ensures that the image is not presented until all commands for the text overlay have been sumbitted and executed
    // Will be inserted after the render complete semaphore if the text overlay is enabled
    VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphores.textOverlayComplete));

    // Set up submit info structure
    // Semaphores will stay the same during application lifetime
    // Command buffer submission info is set by each example
    submitInfo = vkTools::initializers::submitInfo();
    submitInfo.pWaitDstStageMask = &submitPipelineStages;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &semaphores.presentComplete;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &semaphores.renderComplete;
}

#if defined(__ANDROID__)
int32_t VulkanExampleBase::handleAppInput(struct android_app* app, AInputEvent* event) {
    VulkanExampleBase* vulkanExample = (VulkanExampleBase*)app->userData;
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
        if (AInputEvent_getSource(event) == AINPUT_SOURCE_JOYSTICK) {
            // Left thumbstick
            vulkanExample->gamePadState.axisLeft.x = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_X, 0);
            vulkanExample->gamePadState.axisLeft.y = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_Y, 0);
            // Right thumbstick
            vulkanExample->gamePadState.axisRight.x = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_Z, 0);
            vulkanExample->gamePadState.axisRight.y = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_RZ, 0);
        } else {
            // todo : touch input
        }
        return 1;
    }

    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_KEY) {
        int32_t keyCode = AKeyEvent_getKeyCode((const AInputEvent*)event);
        int32_t action = AKeyEvent_getAction((const AInputEvent*)event);
        int32_t button = 0;

        if (action == AKEY_EVENT_ACTION_UP)
            return 0;

        switch (keyCode) {
        case AKEYCODE_BUTTON_A:
            vulkanExample->keyPressed(GAMEPAD_BUTTON_A);
            break;
        case AKEYCODE_BUTTON_B:
            vulkanExample->keyPressed(GAMEPAD_BUTTON_B);
            break;
        case AKEYCODE_BUTTON_X:
            vulkanExample->keyPressed(GAMEPAD_BUTTON_X);
            break;
        case AKEYCODE_BUTTON_Y:
            vulkanExample->keyPressed(GAMEPAD_BUTTON_Y);
            break;
        case AKEYCODE_BUTTON_L1:
            vulkanExample->keyPressed(GAMEPAD_BUTTON_L1);
            break;
        case AKEYCODE_BUTTON_R1:
            vulkanExample->keyPressed(GAMEPAD_BUTTON_R1);
            break;
        case AKEYCODE_BUTTON_START:
            vulkanExample->keyPressed(GAMEPAD_BUTTON_START);
            break;
        };

        LOGD("Button %d pressed", keyCode);
    }

    return 0;
}

void VulkanExampleBase::handleAppCommand(android_app * app, int32_t cmd) {
    assert(app->userData != NULL);
    VulkanExampleBase* vulkanExample = (VulkanExampleBase*)app->userData;
    switch (cmd) {
    case APP_CMD_SAVE_STATE:
        LOGD("APP_CMD_SAVE_STATE");
        /*
        vulkanExample->app->savedState = malloc(sizeof(struct saved_state));
        *((struct saved_state*)vulkanExample->app->savedState) = vulkanExample->state;
        vulkanExample->app->savedStateSize = sizeof(struct saved_state);
        */
        break;
    case APP_CMD_INIT_WINDOW:
        LOGD("APP_CMD_INIT_WINDOW");
        if (vulkanExample->androidApp->window != NULL) {
            vulkanExample->initVulkan(false);
            vulkanExample->initSwapchain();
            vulkanExample->prepare();
            assert(vulkanExample->prepared);
        } else {
            LOGE("No window assigned!");
        }
        break;
    case APP_CMD_LOST_FOCUS:
        LOGD("APP_CMD_LOST_FOCUS");
        vulkanExample->focused = false;
        break;
    case APP_CMD_GAINED_FOCUS:
        LOGD("APP_CMD_GAINED_FOCUS");
        vulkanExample->focused = true;
        break;
    }
}
#else

void VulkanExampleBase::keyPressBase(uint32_t key) {
    switch (key) {
    case GLFW_KEY_P:
        paused = !paused;
        break;
    case VK_F1:
        if (enableTextOverlay) {
            textOverlay->visible = !textOverlay->visible;
        }
        break;
    case VK_ESCAPE:
        PostQuitMessage(0);
        break;
    }

    if (camera.firstperson) {
        switch (key) {
        case GLFW_KEY_W:
            camera.keys.up = true;
            break;
        case GLFW_KEY_S:
            camera.keys.down = true;
            break;
        case GLFW_KEY_A:
            camera.keys.left = true;
            break;
        case GLFW_KEY_D:
            camera.keys.right = true;
            break;
        }
    }

    keyPressed((uint32_t)key);
}

void VulkanExampleBase::keyReleaseBase(uint32_t key) {
    if (camera.firstperson) {
        switch (key) {
        case GLFW_KEY_W:
            camera.keys.up = false;
            break;
        case GLFW_KEY_S:
            camera.keys.down = false;
            break;
        case GLFW_KEY_A:
            camera.keys.left = false;
            break;
        case GLFW_KEY_D:
            camera.keys.right = false;
            break;
        }
    }
}
void VulkanExampleBase::KeyboardHandler(GLFWwindow* window, int key, int scancode, int action, int mods) {
    VulkanExampleBase* example = (VulkanExampleBase*)glfwGetWindowUserPointer(window);
    if (action == GLFW_PRESS) {
        example->keyPressBase(key);
    } else if (action == GLFW_RELEASE) {
        example->keyReleaseBase(key);
    }
}

void VulkanExampleBase::MouseHandler(GLFWwindow* window, int button, int action, int mods) {
    VulkanExampleBase* example = (VulkanExampleBase*)glfwGetWindowUserPointer(window);
    if (action == GLFW_PRESS) {
        glm::dvec2 mousePos; glfwGetCursorPos(window, &mousePos.x, &mousePos.y);
        example->mousePos = mousePos;
    } 
}

// Keyboard movement handler
void VulkanExampleBase::mouseMoved(const glm::vec2& newPos) {
    glm::vec2 deltaPos = mousePos - newPos;
    if (deltaPos.x == 0 && deltaPos.y == 0) {
        return;
    }
    if (GLFW_PRESS == glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT)) {
        zoom += deltaPos.y * .005f * zoomSpeed;
        camera.translate(glm::vec3(0.0f, 0.0f, deltaPos.y * .005f * zoomSpeed));
        mousePos = newPos;
        viewChanged();
    }
    if (GLFW_PRESS == glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)) {
        glm::vec3 deltaPos3 = glm::vec3(deltaPos.y, -deltaPos.x, 0.0f)  * 1.25f * rotationSpeed;
        rotation += deltaPos3;
        camera.rotate(deltaPos3);
        mousePos = newPos;
        viewChanged();
    }
    if (GLFW_PRESS == glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE)) {
        glm::vec3 deltaPos3 = glm::vec3(deltaPos, 0.0f) * 0.01f;
        cameraPos -= deltaPos3;
        camera.translate(-deltaPos3);
        mousePos = newPos;
        viewChanged();
    }
}

void VulkanExampleBase::MouseMoveHandler(GLFWwindow* window, double posx, double posy) {
    VulkanExampleBase* example = (VulkanExampleBase*)glfwGetWindowUserPointer(window);
    example->mouseMoved(glm::vec2(posx, posy));
}

void VulkanExampleBase::MouseScrollHandler(GLFWwindow* window, double xoffset, double yoffset) {
    VulkanExampleBase* example = (VulkanExampleBase*)glfwGetWindowUserPointer(window);
    example->mouseScrolled(yoffset);
}

void VulkanExampleBase::mouseScrolled(float wheelDelta) {
    zoom += (float)wheelDelta * 0.005f * zoomSpeed;
    camera.translate(glm::vec3(0.0f, 0.0f, (float)wheelDelta * 0.005f * zoomSpeed));
    viewChanged();
}

void VulkanExampleBase::CloseHandler(GLFWwindow* window) {
    VulkanExampleBase* example = (VulkanExampleBase*)glfwGetWindowUserPointer(window);
    example->prepared = false;
    glfwSetWindowShouldClose(window, 1);
}

void VulkanExampleBase::FramebufferSizeHandler(GLFWwindow* window, int width, int height) {
    VulkanExampleBase* example = (VulkanExampleBase*)glfwGetWindowUserPointer(window);
    example->windowResize(glm::uvec2(width, height));
}

// Win32 : Sets up a console window and redirects standard output to it
void VulkanExampleBase::setupConsole(std::string title) {
#if defined(_WIN32)
    AllocConsole();
    AttachConsole(GetCurrentProcessId());
    FILE *stream;
    freopen_s(&stream, "CONOUT$", "w+", stdout);
    SetConsoleTitle(TEXT(title.c_str()));
    if (enableValidation) {
        std::cout << "Validation enabled:\n";
    }
#endif
}

void VulkanExampleBase::setupWindow() {
    bool fullscreen = false;

#ifdef _WIN32
    // Check command line arguments
    for (int32_t i = 0; i < __argc; i++) {
        if (__argv[i] == std::string("-fullscreen")) {
            fullscreen = true;
        }
    }
#endif

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    std::string windowTitle = getWindowTitle();
    if (fullscreen) {
        auto monitor = glfwGetPrimaryMonitor();
        auto mode = glfwGetVideoMode(monitor);
        width = mode->width;
        height = mode->height;
        window = glfwCreateWindow(width, height, windowTitle.c_str(), monitor, NULL);
    } else {
        window = glfwCreateWindow(width, height, windowTitle.c_str(), NULL, NULL);
    }

    if (!window) {
        printf("Could not create window!\n");
        fflush(stdout);
        exit(1);
    }

    glfwSetWindowUserPointer(window, this);
    glfwSetKeyCallback(window, KeyboardHandler);
    glfwSetMouseButtonCallback(window, MouseHandler);
    glfwSetCursorPosCallback(window, MouseMoveHandler);
    glfwSetWindowCloseCallback(window, CloseHandler);
    glfwSetFramebufferSizeCallback(window, FramebufferSizeHandler);
    glfwSetScrollCallback(window, MouseScrollHandler);
}

#endif

void VulkanExampleBase::viewChanged() {
    // Can be overrdiden in derived class
}

void VulkanExampleBase::keyPressed(uint32_t keyCode) {
    // Can be overriden in derived class
}

void VulkanExampleBase::buildCommandBuffers() {
    // Can be overriden in derived class
}

void VulkanExampleBase::buildPresentCommandBuffers() {
    VkCommandBufferBeginInfo cmdBufInfo = vkTools::initializers::commandBufferBeginInfo();

    for (uint32_t i = 0; i < swapChain.imageCount; i++) {
        // Command buffer for post present barrier

        // Insert a post present image barrier to transform the image back to a
        // color attachment that our render pass can write to
        // We always use undefined image layout as the source as it doesn't actually matter
        // what is done with the previous image contents

        VK_CHECK_RESULT(vkBeginCommandBuffer(postPresentCmdBuffers[i], &cmdBufInfo));

        VkImageMemoryBarrier postPresentBarrier = vkTools::initializers::imageMemoryBarrier();
        postPresentBarrier.srcAccessMask = 0;
        postPresentBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        postPresentBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        postPresentBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        postPresentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        postPresentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        postPresentBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        postPresentBarrier.image = swapChain.buffers[i].image;

        vkCmdPipelineBarrier(
            postPresentCmdBuffers[i],
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &postPresentBarrier);

        VK_CHECK_RESULT(vkEndCommandBuffer(postPresentCmdBuffers[i]));

        // Command buffers for pre present barrier

        // Submit a pre present image barrier to the queue
        // Transforms the (framebuffer) image layout from color attachment to present(khr) for presenting to the swap chain

        VK_CHECK_RESULT(vkBeginCommandBuffer(prePresentCmdBuffers[i], &cmdBufInfo));

        VkImageMemoryBarrier prePresentBarrier = vkTools::initializers::imageMemoryBarrier();
        prePresentBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        prePresentBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        prePresentBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        prePresentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        prePresentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        prePresentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        prePresentBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        prePresentBarrier.image = swapChain.buffers[i].image;

        vkCmdPipelineBarrier(
            prePresentCmdBuffers[i],
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            VK_FLAGS_NONE,
            0, nullptr, // No memory barriers,
            0, nullptr, // No buffer barriers,
            1, &prePresentBarrier);

        VK_CHECK_RESULT(vkEndCommandBuffer(prePresentCmdBuffers[i]));
    }
}

VkBool32 VulkanExampleBase::getMemoryType(uint32_t typeBits, VkFlags properties, uint32_t * typeIndex) {
    for (uint32_t i = 0; i < 32; i++) {
        if ((typeBits & 1) == 1) {
            if ((deviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                *typeIndex = i;
                return true;
            }
        }
        typeBits >>= 1;
    }
    return false;
}

uint32_t VulkanExampleBase::getMemoryType(uint32_t typeBits, VkFlags properties) {
    for (uint32_t i = 0; i < 32; i++) {
        if ((typeBits & 1) == 1) {
            if ((deviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
        typeBits >>= 1;
    }

    // todo : throw error
    return 0;
}

void VulkanExampleBase::createCommandPool() {
    VkCommandPoolCreateInfo cmdPoolInfo = {};
    cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolInfo.queueFamilyIndex = swapChain.queueNodeIndex;
    cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VK_CHECK_RESULT(vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &cmdPool));
}

void VulkanExampleBase::setupDepthStencil() {
    VkImageCreateInfo image = {};
    image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image.pNext = NULL;
    image.imageType = VK_IMAGE_TYPE_2D;
    image.format = depthFormat;
    image.extent = { width, height, 1 };
    image.mipLevels = 1;
    image.arrayLayers = 1;
    image.samples = VK_SAMPLE_COUNT_1_BIT;
    image.tiling = VK_IMAGE_TILING_OPTIMAL;
    image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    image.flags = 0;

    VkMemoryAllocateInfo mem_alloc = {};
    mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mem_alloc.pNext = NULL;
    mem_alloc.allocationSize = 0;
    mem_alloc.memoryTypeIndex = 0;

    VkImageViewCreateInfo depthStencilView = {};
    depthStencilView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    depthStencilView.pNext = NULL;
    depthStencilView.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depthStencilView.format = depthFormat;
    depthStencilView.flags = 0;
    depthStencilView.subresourceRange = {};
    depthStencilView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    depthStencilView.subresourceRange.baseMipLevel = 0;
    depthStencilView.subresourceRange.levelCount = 1;
    depthStencilView.subresourceRange.baseArrayLayer = 0;
    depthStencilView.subresourceRange.layerCount = 1;

    VkMemoryRequirements memReqs;

    VK_CHECK_RESULT(vkCreateImage(device, &image, nullptr, &depthStencil.image));
    vkGetImageMemoryRequirements(device, depthStencil.image, &memReqs);
    mem_alloc.allocationSize = memReqs.size;
    mem_alloc.memoryTypeIndex = getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK_RESULT(vkAllocateMemory(device, &mem_alloc, nullptr, &depthStencil.mem));

    VK_CHECK_RESULT(vkBindImageMemory(device, depthStencil.image, depthStencil.mem, 0));
    vkTools::setImageLayout(
        setupCmdBuffer,
        depthStencil.image,
        VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    depthStencilView.image = depthStencil.image;
    VK_CHECK_RESULT(vkCreateImageView(device, &depthStencilView, nullptr, &depthStencil.view));
}

void VulkanExampleBase::setupFrameBuffer() {
    VkImageView attachments[2];

    // Depth/Stencil attachment is the same for all frame buffers
    attachments[1] = depthStencil.view;

    VkFramebufferCreateInfo frameBufferCreateInfo = {};
    frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    frameBufferCreateInfo.pNext = NULL;
    frameBufferCreateInfo.renderPass = renderPass;
    frameBufferCreateInfo.attachmentCount = 2;
    frameBufferCreateInfo.pAttachments = attachments;
    frameBufferCreateInfo.width = width;
    frameBufferCreateInfo.height = height;
    frameBufferCreateInfo.layers = 1;

    // Create frame buffers for every swap chain image
    frameBuffers.resize(swapChain.imageCount);
    for (uint32_t i = 0; i < frameBuffers.size(); i++) {
        attachments[0] = swapChain.buffers[i].view;
        VK_CHECK_RESULT(vkCreateFramebuffer(device, &frameBufferCreateInfo, nullptr, &frameBuffers[i]));
    }
}

void VulkanExampleBase::setupRenderPass() {
    VkAttachmentDescription attachments[2] = {};

    // Color attachment
    attachments[0].format = colorformat;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Depth attachment
    attachments[1].format = depthFormat;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorReference = {};
    colorReference.attachment = 0;
    colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthReference = {};
    depthReference.attachment = 1;
    depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.flags = 0;
    subpass.inputAttachmentCount = 0;
    subpass.pInputAttachments = NULL;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorReference;
    subpass.pResolveAttachments = NULL;
    subpass.pDepthStencilAttachment = &depthReference;
    subpass.preserveAttachmentCount = 0;
    subpass.pPreserveAttachments = NULL;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.pNext = NULL;
    renderPassInfo.attachmentCount = 2;
    renderPassInfo.pAttachments = attachments;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 0;
    renderPassInfo.pDependencies = NULL;

    VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass));
}

void VulkanExampleBase::windowResize(const glm::uvec2& newSize) {
    if (!prepared) {
        return;
    }
    prepared = false;

    // Recreate swap chain
    width = newSize.x;
    height = newSize.y;
    createSetupCommandBuffer();
    setupSwapChain();

    // Recreate the frame buffers

    vkDestroyImageView(device, depthStencil.view, nullptr);
    vkDestroyImage(device, depthStencil.image, nullptr);
    vkFreeMemory(device, depthStencil.mem, nullptr);
    setupDepthStencil();

    for (uint32_t i = 0; i < frameBuffers.size(); i++) {
        vkDestroyFramebuffer(device, frameBuffers[i], nullptr);
    }
    setupFrameBuffer();

    flushSetupCommandBuffer();

    // Command buffers need to be recreated as they may store
    // references to the recreated frame buffer
    destroyCommandBuffers();
    createCommandBuffers();
    buildCommandBuffers();
    buildPresentCommandBuffers();

    vkQueueWaitIdle(queue);
    vkDeviceWaitIdle(device);

    if (enableTextOverlay) {
        textOverlay->reallocateCommandBuffers();
        updateTextOverlay();
    }

    camera.updateAspectRatio((float)width / (float)height);

    // Notify derived class
    windowResized();
    viewChanged();

    prepared = true;
}

void VulkanExampleBase::windowResized() {
    // Can be overriden in derived class
}

void VulkanExampleBase::initSwapchain() {
#if defined(__ANDROID__)	
    swapChain.initSurface(androidApp->window);
#else
    swapChain.initSurface(instance, window);
#endif
}

void VulkanExampleBase::setupSwapChain() {
    swapChain.create(setupCmdBuffer, &width, &height, enableVSync);
}
