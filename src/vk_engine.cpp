//> includes
#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_initializers.h>
#include <vk_types.h>

//bootstrap library
#include "VkBootstrap.h"

#include <chrono>
#include <thread>

VulkanEngine* loadedEngine = nullptr;
VulkanEngine& VulkanEngine::Get() { return *loadedEngine; }

constexpr bool bUseValidationLayers = true;

void VulkanEngine::Init()
{
    // only one engine initialization is allowed with the application.
    assert(loadedEngine == nullptr);
    loadedEngine = this;

    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

    mWindow = SDL_CreateWindow(
        "Vulkan Engine",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        mWindowExtent.width,
        mWindowExtent.height,
        window_flags);

	InitVulkan();
	InitSwapchain();
	InitCommands();
	InitSyncStructures();

    // everything went fine
    mIsInitialized = true;
}

void VulkanEngine::InitVulkan()
{
	vkb::InstanceBuilder builder;

	//make the vulkan instance, with basic debug features
	auto inst_ret = builder.set_app_name("Example Vulkan Application")
		.request_validation_layers(bUseValidationLayers)
		.use_default_debug_messenger()
		.require_api_version(1, 3, 0)
		.build();

	vkb::Instance vkb_inst = inst_ret.value();

	// grab the instance
	mInstance = vkb_inst.instance;
	mDebugMessenger = vkb_inst.debug_messenger;

	SDL_Vulkan_CreateSurface(mWindow, mInstance, &mSurface);

	// vulkan 1.3 features
	VkPhysicalDeviceVulkan13Features features13{};
	features13.dynamicRendering = true;
	features13.synchronization2 = true;

	// vulkan 1.2 features
	VkPhysicalDeviceVulkan12Features features12{};
	features12.bufferDeviceAddress = true;
	features12.descriptorIndexing = true;

	// use vkbootstrap to select a gpu.
	// We want a gpu that can write to the SDL surface and supports vulkan 1.3 with the correct features
	vkb::PhysicalDeviceSelector selector{ vkb_inst };
	vkb::PhysicalDevice physicalDevice = selector
		.set_minimum_version(1, 3)
		.set_required_features_13(features13)
		.set_required_features_12(features12)
		.set_surface(mSurface)
		.select()
		.value();

	// Create the final vulkan device
	vkb::DeviceBuilder deviceBuilder{ physicalDevice};
	vkb::Device vkbDevice = deviceBuilder.build().value();

	// Get the VkDevice handle used in the rest of the vulkan application
	mDevice = vkbDevice.device;
	mChosenGPU = physicalDevice.physical_device;

	// use vkbootstrap to get a Graphics queue
	mGraphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
	mGraphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
}

void VulkanEngine::InitSwapchain()
{
	CreateSwapchain(mWindowExtent.width, mWindowExtent.height);
}

void VulkanEngine::CreateSwapchain(uint32_t width, uint32_t height)
{
	vkb::SwapchainBuilder swapchainBuilder{ mChosenGPU, mDevice, mSurface};
	mSwapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

	vkb::Swapchain vkbSwapchain = swapchainBuilder
		// use default_format_selection()
		.set_desired_format(VkSurfaceFormatKHR{ .format = mSwapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
		// use vysnc present mode
		// The most important detail here is the present mode, which we have set to VK_PRESENT_MODE_FIFO_KHR. 
		// This way we are doing a hard VSync, which will limit the FPS of the entire engine to the speed of the monitor.
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		.set_desired_extent(width, height)
		.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		.build()
		.value();

	mSwapchainExtent = vkbSwapchain.extent;
	// store swapchain and its related iamges
	mSwapchain = vkbSwapchain.swapchain;
	mSwapchainImages = vkbSwapchain.get_images().value();
	mSwapchainImageViews = vkbSwapchain.get_image_views().value();
}

void VulkanEngine::DestroySwapchain()
{
	// We first delete the swapchain object, which will delete the images it holds internally. 
	//  We then have to destroy the ImageViews for those images.

	vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);

	// destroy swapchain resources
	for (int i = 0; i < mSwapchainImageViews.size(); i++) {

		vkDestroyImageView(mDevice, mSwapchainImageViews[i], nullptr);
	}
}

void VulkanEngine::InitCommands()
{
	//create a command pool for commands submitted to the graphics queue.
	//we also want the pool to allow for resetting of individual command buffers
	VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(mGraphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	for (int i = 0; i < FRAME_OVERLAP; i++) {

		VK_CHECK(vkCreateCommandPool(mDevice, &commandPoolInfo, nullptr, &mFrames[i].mCommandPool));

		// allocate the default command buffer that we will use for rendering
		VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(mFrames[i].mCommandPool, 1);

		VK_CHECK(vkAllocateCommandBuffers(mDevice, &cmdAllocInfo, &mFrames[i].mMainCommandBuffer));
	}
}

void VulkanEngine::InitSyncStructures()
{

}

void VulkanEngine::Cleanup()
{
    if (mIsInitialized) {

		//make sure the gpu has stopped doing its things
		vkDeviceWaitIdle(mDevice);

		for (int i = 0; i < FRAME_OVERLAP; i++) {
			vkDestroyCommandPool(mDevice, mFrames[i].mCommandPool, nullptr);
			// This will also destroy all CommandBuffers linked to the CommandPool
		}


		DestroySwapchain();

		vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
		vkDestroyDevice(mDevice, nullptr);

		// VkPhysicalDevice can’t be destroyed, as it’s not a Vulkan resource per-se, it’s more like just a handle to a GPU in the system.

		vkb::destroy_debug_utils_messenger(mInstance, mDebugMessenger);
		vkDestroyInstance(mInstance, nullptr);
		
		SDL_DestroyWindow(mWindow);
    }

    // clear engine pointer
    loadedEngine = nullptr;
}

void VulkanEngine::Draw()
{
    // nothing yet
}

void VulkanEngine::Run()
{
    SDL_Event e;
    bool bQuit = false;

    // main loop
    while (!bQuit) {
        // Handle events on queue
        while (SDL_PollEvent(&e) != 0) {
            // close the window when user alt-f4s or clicks the X button
            if (e.type == SDL_QUIT)
                bQuit = true;

            if (e.type == SDL_WINDOWEVENT) {
                if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) {
                    mStopRendering = true;
                }
                if (e.window.event == SDL_WINDOWEVENT_RESTORED) {
                    mStopRendering = false;
                }
            }
        }

        // do not draw if we are minimized
        if (mStopRendering) {
            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        Draw();
    }
}