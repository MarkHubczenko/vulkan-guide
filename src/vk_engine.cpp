//> includes
#include "vk_engine.h"
#include "vk_images.h"

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
	// Create syncronization structures
	// one fence to control when the gpu has finished rendering the frame,
	// and 2 semaphores to synchronize rendering with swapchain.
	// we want the fence to start signalled so we can wait on it on the first frame

	// On the fence, we are using the flag VK_FENCE_CREATE_SIGNALED_BIT . 
	// This is very important, as it allows us to wait on a freshly created fence without causing errors. 
	// If we did not have that bit, when we call into WaitFences the first frame, before the gpu is doing work, the thread will be blocked.
	VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
	VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

	for (int i = 0; i < FRAME_OVERLAP; i++)
	{
		VK_CHECK(vkCreateFence(mDevice, &fenceCreateInfo, nullptr, &mFrames[i].mRenderFence));
		VK_CHECK(vkCreateSemaphore(mDevice, &semaphoreCreateInfo, nullptr, &mFrames[i].mSwapchainSemaphore));
		VK_CHECK(vkCreateSemaphore(mDevice, &semaphoreCreateInfo, nullptr, &mFrames[i].mRenderSemaphore));
	}
}

void VulkanEngine::Cleanup()
{
    if (mIsInitialized) {

		//make sure the gpu has stopped doing its things
		vkDeviceWaitIdle(mDevice);

		for (int i = 0; i < FRAME_OVERLAP; i++) {
			vkDestroyCommandPool(mDevice, mFrames[i].mCommandPool, nullptr);
			// This will also destroy all CommandBuffers linked to the CommandPool
			vkDestroyCommandPool(mDevice, mFrames[i].mCommandPool, nullptr);

			//destroy sync objects
			vkDestroyFence(mDevice, mFrames[i].mRenderFence, nullptr);
			vkDestroySemaphore(mDevice, mFrames[i].mRenderSemaphore, nullptr);
			vkDestroySemaphore(mDevice, mFrames[i].mSwapchainSemaphore, nullptr);
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
	// wait until the gpu has finished rendering the last frame. Timeout of 1 second (nanoseconds)
	VK_CHECK(vkWaitForFences(mDevice, 1, &GetCurrentFrame().mRenderFence, true, 1000000000));
	VK_CHECK(vkResetFences(mDevice, 1, &GetCurrentFrame().mRenderFence));

	//request image from the swapchain
	uint32_t swapchainImageIndex;
	// vkAcquireNextImageKHR will request the image index from the swapchain, 
	// and if the swapchain doesn’t have any image we can use, 
	// it will block the thread with a maximum for the timeout set, which will be 1 second.
	VK_CHECK(vkAcquireNextImageKHR(mDevice, mSwapchain, 1000000000, GetCurrentFrame().mSwapchainSemaphore, nullptr, &swapchainImageIndex));

	// naming it cmd for shorter writing
	// Vulkan handles are just a 64 bit handle/pointer, so its fine to copy them around, but remember that their actual data is handled by vulkan itself.
	VkCommandBuffer cmd = GetCurrentFrame().mMainCommandBuffer;

	// now that we are sure that the commands finished executing, we can safely
	// reset the command buffer to begin recording again.
	VK_CHECK(vkResetCommandBuffer(cmd, 0));

	// begin the command buffer recording. We will use this command buffer exactly once, so we want to let vulkan know that
	// we will give it the flag VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT.
	// This is optional, but we might get a small speedup from our command encoding if we can tell the drivers that this buffer will only be 
	// submitted and executed once.
	// We are only doing 1 submit per frame before the command buffer is reset, so this is perfectly good for us.
	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	//start the command buffer recording
	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));


	//make the swapchain image into writeable mode before rendering
	vkutil::transition_image(cmd, mSwapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

	//make a clear-color from frame number. This will flash with a 120 frame period.
	VkClearColorValue clearValue;
	float flash = abs(sin(mFrameNumber / 120.f));
	clearValue = { { 0.0f, 0.0f, flash, 1.0f } };

	VkImageSubresourceRange clearRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);

	//clear image
	vkCmdClearColorImage(cmd, mSwapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);

	//make the swapchain image into presentable mode
	vkutil::transition_image(cmd, mSwapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	//finalize the command buffer (we can no longer add commands, but it can now be executed)
	VK_CHECK(vkEndCommandBuffer(cmd));

	//prepare the submission to the queue. 
	//we want to wait on the mPresentSemaphore, as that semaphore is signaled when the swapchain is ready
	//we will signal the mRenderSemaphore, to signal that rendering has finished

	VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);

	VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, GetCurrentFrame().mSwapchainSemaphore);
	VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, GetCurrentFrame().mRenderSemaphore);

	VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, &signalInfo, &waitInfo);

	//submit command buffer to the queue and execute it.
	// _renderFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit2(mGraphicsQueue, 1, &submit, GetCurrentFrame().mRenderFence));

	//prepare present
	// this will put the image we just rendered to into the visible window.
	// we want to wait on the mRenderSemaphore for that, 
	// as its necessary that drawing commands have finished before the image is displayed to the user
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;
	presentInfo.pSwapchains = &mSwapchain;
	presentInfo.swapchainCount = 1;

	presentInfo.pWaitSemaphores = &GetCurrentFrame().mRenderSemaphore;
	presentInfo.waitSemaphoreCount = 1;

	presentInfo.pImageIndices = &swapchainImageIndex;

	VK_CHECK(vkQueuePresentKHR(mGraphicsQueue, &presentInfo));

	//increase the number of frames drawn
	mFrameNumber++;

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