// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>

struct FrameData {

	VkCommandPool mCommandPool;
	VkCommandBuffer mMainCommandBuffer;
};

constexpr unsigned int FRAME_OVERLAP = 2;


class VulkanEngine {
public:

	bool mIsInitialized{ false };
	int mFrameNumber {0};
	bool mStopRendering{ false };
	VkExtent2D mWindowExtent{ 1700 , 900 };

	struct SDL_Window* mWindow{ nullptr };

	static VulkanEngine& Get();

	//initializes everything in the engine
	void Init();

	//shuts down the engine
	void Cleanup();

	//draw loop
	void Draw();

	//run main loop
	void Run();

	// Vulkan Members
	VkInstance mInstance; // Vulkan library handle
	VkDebugUtilsMessengerEXT mDebugMessenger; // Vulkan debug output handle
	VkPhysicalDevice mChosenGPU; // GPU chosen as the default device
	VkDevice  mDevice; // Vulkan device for commands
	VkSurfaceKHR mSurface; // Vulkan window surface

	// Vulkan Swapchain Members
	VkSwapchainKHR mSwapchain;
	VkFormat mSwapchainImageFormat;

	std::vector<VkImage> mSwapchainImages;
	std::vector<VkImageView> mSwapchainImageViews;
	VkExtent2D mSwapchainExtent;

	// FrameData for holding our command pools
	FrameData mFrames[FRAME_OVERLAP];
	FrameData& GetCurrentFrame() { return mFrames[mFrameNumber % FRAME_OVERLAP]; }

	VkQueue mGraphicsQueue;
	uint32_t mGraphicsQueueFamily;

private:
	void InitVulkan();
	
	void InitSwapchain();
	void CreateSwapchain(uint32_t width, uint32_t height);
	void DestroySwapchain();
	
	void InitCommands();
	void InitSyncStructures();


};
