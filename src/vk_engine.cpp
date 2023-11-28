#include <iostream>																\

#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_types.h>
#include <vk_initializers.h>

//bootstrap library
#include "VkBootstrap.h"

#include "Debug.h"

// we want to immediately abort when there is an error. 
// In normal engines this would give an error message to the user, or perform a dump of state.
#define VK_CHECK(x)													\
do {																	\
	VkResult err = x;												\
	if (err)														\
	{																\
		std::cout << "Detected Vulkan error: " << err << std::endl;	\
		abort();													\
	}																\
} while(0)

void VulkanEngine::init()
{
	// We initialize SDL and create a window with it. 
	SDL_Init(SDL_INIT_VIDEO);

	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);
	
	_window = SDL_CreateWindow(
		"Vulkan Engine",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		_windowExtent.width,
		_windowExtent.height,
		window_flags
	);
	
	// load the core Vulkan structures
	init_vulkan();

	// create the swapchain
	init_swapchain();
	
	// create the command related members
	init_commands();

	// IMPORTANT
	// We need to create the renderpass BEFORE the framebuffers, 
	// because the framebuffers are created for a specific renderpass.

	// create default renderpass
	init_default_renderpass();

	// create frame buffers
	init_framebuffers();

	//everything went fine
	_isInitialized = true;
}
void VulkanEngine::cleanup()
{	
	// Because our initialization order was SDL Window -> Instance -> Surface -> Device -> Swapchain, 
	// we are doing exactly the opposite order for destruction.

	if (_isInitialized) 
	{
		// Destroy command pool (also destroys the command buffers linked to the command pool)
		vkDestroyCommandPool(_device, _commandPool, nullptr);

		// It is imperative that objects are destroyed in the opposite order that they are created. 
		// In some cases, if you know what you are doing, the order can be changed a bit and it will be fine, 
		// but destroying the objects in reverse order is an easy way to have it work.
		vkDestroySwapchainKHR(_device, _swapchain, nullptr);
		
		//destroy the main renderpass
		vkDestroyRenderPass(_device, _renderPass, nullptr);

		// destroy swapchain resources
		for (int i = 0; i < _swapchainImageViews.size(); i++)
		{
			vkDestroyFramebuffer(_device, _framebuffers[i], nullptr);
			vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
		}
		
		// There is no need to destroy the Images in this specific case, because the images are owned and destroyed with the swapchain.

		vkDestroyDevice(_device, nullptr);
		vkDestroySurfaceKHR(_instance, _surface, nullptr);
		vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
		vkDestroyInstance(_instance, nullptr);
		SDL_DestroyWindow(_window);
	}

	// VkPhysicalDevice can’t be destroyed, as it’s not a Vulkan resource per-se, it’s more like just a handle to a GPU in the system.
	
}

void VulkanEngine::draw()
{
	//nothing yet
}

void VulkanEngine::run()
{
	SDL_Event e;
	bool bQuit = false;

	//main loop
	while (!bQuit)
	{
		//Handle events on queue
		while (SDL_PollEvent(&e) != 0)
		{
			switch(e.type)
			{
				case(SDL_QUIT):
					bQuit = true;
					break;
				case(SDL_KEYDOWN):
				case(SDL_KEYUP):
					Debug::PrintKeyInfo(&e.key);
					break;
				default:
					break;
			}
			//close the window when user alt-f4s or clicks the X button			
			if (e.type == SDL_QUIT) bQuit = true;
		}

		draw();
	}
}

void VulkanEngine::init_vulkan()
{
	// Instance builder from the VkBootstrap library to simplify 
	// the creation of the Vulkan VkInstance.
	vkb::InstanceBuilder builder;

	// Make the Vulkan instance, with basic debug features
	// vkb::detail::Result<vkb::Instance>
	auto inst_ret = builder.set_app_name("Example Vulkan Application")
		.request_validation_layers(true) // In a more complex project we would only want to enable this in debug mode 
		.require_api_version(1, 1, 0) // Vulkan api v 1.1
		.use_default_debug_messenger()
		.build();

	vkb::Instance vkb_inst = inst_ret.value();

	// Store the instance
	_instance = vkb_inst.instance;
	// Store the debug messenger
	_debug_messenger = vkb_inst.debug_messenger;

	// Get the surface of the window we opened with SDL
	SDL_Vulkan_CreateSurface(_window, _instance, &_surface);

	// Use vkbootstrap to select a GPU
	// We want a GPU that can write to the SDL surface and support Vulkan 1.1
	vkb::PhysicalDeviceSelector selector{vkb_inst};
	vkb::PhysicalDevice physicalDevice = selector
		.set_minimum_version(1, 1)
		.set_surface(_surface)
		.select()
		.value();

	// Create the final Vulkan device
	vkb::DeviceBuilder deviceBuilder{physicalDevice};
	vkb::Device vkbDevice = deviceBuilder.build().value();

	// Get the VkDevice handle used in the rest of a Vulkan application
	_device = vkbDevice.device;
	_chosenGPU = physicalDevice.physical_device;

	// use vkbootstrap to get a Graphics queue
	_graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
	_graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
}

void VulkanEngine::init_swapchain()
{
	vkb::SwapchainBuilder swapchainBuilder{_chosenGPU, _device, _surface};
	vkb::Swapchain vkbSwapchain = swapchainBuilder
		.use_default_format_selection()
		// use vsync present mode
		// Can read about swapchain types here https://vkguide.dev/docs/chapter-1/vulkan_init_flow/ 
		// Probably switch to mailbox for other projects
		// This way we are doing a hard VSync, which will limit the FPS of the entire engine to the speed of the monitor. It’s a good way to have a FPS limit for now.
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		// We also send the window sizes to the swapchain. 
		// This is important as creating a swapchain will also create the images for it, so the size is locked. 
		// If you need to resize the window, the swapchain will need to be rebuild.
		.set_desired_extent(_windowExtent.width, _windowExtent.height)
		.build()
		.value();

	// store swapchain and its related images
	_swapchain = vkbSwapchain.swapchain;
	_swapchainImages = vkbSwapchain.get_images().value();
	_swapchainImageViews = vkbSwapchain.get_image_views().value();
	_swapchainImageFormat = vkbSwapchain.image_format;
}

void VulkanEngine::init_commands()
{
	// create a command pool for commands submitted to the graphics queue.
	// we also want the pool to allow for resetting of individual command buffers
	// By doing that ` = {}` thing, we are letting the compiler initialize the entire struct to zero. 
	// This is critical, as in general Vulkan structs will have their defaults set in a way that 0 is relatively safe.
	// By doing that, we make sure we don’t leave uninitialized data in the struct.
	VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_commandPool));

	// allocate the default command buffer that we will use for rendering
	VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_commandPool, 1);

	VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_mainCommandBuffer));
}

void VulkanEngine::init_default_renderpass()
{
	// The color attachment is the description of the image we will be writing into with rendering commands
	// the renderpass will use this color attachment.
	VkAttachmentDescription color_attachment = {};
	// the attachment will have hte format need by the swapchain
	color_attachment.format = _swapchainImageFormat;
	// 1 sample, we won't be doing MSAA (Multi-Sample Anti-Aliasing)
	color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	// We clear when this attachment is loaded
	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	// We keep the attachment stored when the renderpass ends
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	// we don't care about stencil
	color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	// we don't know or care about the starting layout of the attachment
	color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	// after the renderpass ends, the image has to be on a layout ready for display
	color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;


	// Now that our main image target is defined, we need to add a subpass that will render into it. 
	// This goes right after defining the attachment

	VkAttachmentReference color_attachment_ref = {};
	// attachment number will index into the pAttachments array in the parent renderpass itself
	color_attachment_ref.attachment = 0;
	color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	//we are going to create 1 subpass, which is the minimum you can do
	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_ref;

	// The image life will follow something like this
	// UNDEFINED -> RenderPass Begins -> Subpass 0 begins (Transition to Attachment Optimal) 
	// -> Subpass 0 renders -> Subpass 0 ends -> Renderpass Ends (Transitions to Present Source) 

	// The Vulkan driver will perform the layout transitions for us when using the renderpass. 
	// If we weren’t using a renderpass (drawing from compute shaders) we would need to do the same transitions explicitly.

	// Now that the main attachment and the subpass is done, we can create the renderpass
	VkRenderPassCreateInfo render_pass_info = {};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

	//connect the color attachment to the info
	render_pass_info.attachmentCount = 1;
	render_pass_info.pAttachments = &color_attachment;
	//connect the subpass to the info
	render_pass_info.subpassCount = 1;
	render_pass_info.pSubpasses = &subpass;

	VK_CHECK(vkCreateRenderPass(_device, &render_pass_info, nullptr, &_renderPass));
}

void VulkanEngine::init_framebuffers()
{
	//create the framebuffers for the swapchain images. This will connect the render-pass to the images for rendering
	VkFramebufferCreateInfo fb_info = {};
	fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fb_info.pNext = nullptr;

	fb_info.renderPass = _renderPass;
	fb_info.attachmentCount = 1;
	fb_info.width = _windowExtent.width;
	fb_info.height = _windowExtent.height;
	fb_info.layers = 1;

	//grab how many images we have in the swapchain
	const uint32_t swapchain_imagecount = (uint32_t)_swapchainImages.size();
	_framebuffers = std::vector<VkFramebuffer>((uint32_t)swapchain_imagecount);

	//create framebuffers for each of the swapchain image views
	for (uint32_t i = 0; i < swapchain_imagecount; i++) {

		fb_info.pAttachments = &_swapchainImageViews[i];
		VK_CHECK(vkCreateFramebuffer(_device, &fb_info, nullptr, &_framebuffers[i]));
	}
}


