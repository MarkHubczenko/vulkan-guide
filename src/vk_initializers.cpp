#include <vk_initializers.h>

VkCommandPoolCreateInfo vkinit::command_pool_create_info(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags /*= 0*/)
{
	// By doing that ` = {}` thing, we are letting the compiler initialize the entire struct to zero. 
	// This is critical, as in general Vulkan structs will have their defaults set in a way that 0 is relatively safe.
	// By doing that, we make sure we don’t leave uninitialized data in the struct.
	VkCommandPoolCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	info.pNext = nullptr;

	info.queueFamilyIndex = queueFamilyIndex;
	info.flags = flags;
	return info;
}

VkCommandBufferAllocateInfo vkinit::command_buffer_allocate_info(VkCommandPool pool, uint32_t count /*= 1*/, VkCommandBufferLevel level /*= VK_COMMAND_BUFFER_LEVEL_PRIMARY*/)
{
	VkCommandBufferAllocateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	info.pNext = nullptr;

	info.commandPool = pool;
	info.commandBufferCount = count;
	// Command buffers can be Primary of Secondary level. 
	// Primary level are the ones that are sent into a VkQueue, and do all of the work. 
	// Secondary level are ones that can act as “subcommands” to a primary buffer. 
	// They are most commonly used in advanced multithreading scenarios.
	info.level = level;
	return info;
}
