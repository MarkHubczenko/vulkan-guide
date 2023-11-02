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
{																	\
	VkResult err = x;												\
	if (err)														\
	{																\
		std::cout << "Detected Vulkan error: " err << std::endl;	\
		abort();													\
	}																\
}

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

	//everything went fine
	_isInitialized = true;
}
void VulkanEngine::cleanup()
{	
	// Because our initialization order was SDL Window -> Instance -> Surface -> Device -> Swapchain, 
	// we are doing exactly the opposite order for destruction.

	if (_isInitialized) 
	{
		// It is imperative that objects are destroyed in the opposite order that they are created. 
		// In some cases, if you know what you are doing, the order can be changed a bit and it will be fine, 
		// but destroying the objects in reverse order is an easy way to have it work.
		vkDestroySwapchainKHR(_device, _swapchain, nullptr);

		// destroy swapchain resources
		for (int i = 0; i < _swapchainImageViews.size(); i++)
		{
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

