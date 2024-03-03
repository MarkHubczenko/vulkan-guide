#include <vk_images.h>
#include <vk_initializers.h>

void vkutil::transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout)
{
	// VkImageMemoryBarrier2 contains the information for a given image barrier. 
	// On here, is where we set the old and new layouts. 
	VkImageMemoryBarrier2 imageBarrier{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
	imageBarrier.pNext = nullptr;

	// In the StageMask, we are doing ALL_COMMANDS. 
	// This is inefficient, as it will stall the GPU pipeline a bit. 
	// For our needs, its going to be fine as we are only going to do a few transitions per frame. 
	// If you are doing many transitions per frame as part of a post-process chain, 
	// you want to avoid doing this, and instead use StageMasks more accurate to what you are doing.
	// More reading on this
	// https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples
	imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
	imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

	imageBarrier.oldLayout = currentLayout;
	imageBarrier.newLayout = newLayout;

	// As part of the barrier, we need to use a VkImageSubresourceRange too. 
	// This lets us target a part of the image with the barrier. 
	// Its most useful for things like array images or mipmapped images, 
	// where we would only need to barrier on a given layer or mipmap level. 
	// We are going to completely default it and have it transition all mipmap levels and layers.
	VkImageAspectFlags aspectMask = (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	imageBarrier.subresourceRange = vkinit::image_subresource_range(aspectMask);
	imageBarrier.image = image;

	VkDependencyInfo depInfo{};
	depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	depInfo.pNext = nullptr;

	depInfo.imageMemoryBarrierCount = 1;
	depInfo.pImageMemoryBarriers = &imageBarrier;

	vkCmdPipelineBarrier2(cmd, &depInfo);
}
