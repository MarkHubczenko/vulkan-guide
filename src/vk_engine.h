// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>
#include <vector>

class VulkanEngine {
public:

	bool _isInitialized{ false };
	int _frameNumber {0};

	VkExtent2D _windowExtent{ 1700 , 900 };

	struct SDL_Window* _window{ nullptr };

	//initializes everything in the engine
	void init();

	//shuts down the engine
	void cleanup();

	//draw loop
	void draw();

	//run main loop
	void run();

	// Vulkan Handles
	VkInstance _instance; // Vulkan library handle
	VkDebugUtilsMessengerEXT _debug_messenger; // Vulkan debug output handle
	VkPhysicalDevice _chosenGPU; // GPU chosen as the default device
	VkDevice _device; // Vulkan device for commands
	VkSurfaceKHR _surface; // Vulkan window surface

	// Swapchain members
	// Can read about swapchain types here https://vkguide.dev/docs/chapter-1/vulkan_init_flow/
	VkSwapchainKHR _swapchain; 
	VkFormat _swapchainImageFormat; // image format expected by the windowing system
	std::vector<VkImage> _swapchainImages; // array of images from the swapchain
	std::vector<VkImageView> _swapchainImageViews; // array of image views from the swapchain

	// Commands members
	// Command flow https://vkguide.dev/docs/chapter-1/vulkan_command_flow/ 
	VkQueue _graphicsQueue; // Queue we will submit to
	uint32_t _graphicsQueueFamily; // Family of that queue

	VkCommandPool _commandPool; // The command pool for our commands
	// Read about command buffers here https://registry.khronos.org/vulkan/specs/1.2-extensions/html/chap6.html#commandbuffers-lifecycle 
	VkCommandBuffer _mainCommandBuffer; // The Buffer we will record into

private:
	// Setup vulkan handles 
	void init_vulkan();
	// Setup swapchain
	void init_swapchain();
	// Setup commands
	void init_commands();
};
