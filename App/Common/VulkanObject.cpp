#include "VulkanObject.hpp"

#include "ErrorHandler.hpp"

#include <cassert>

using std::make_pair;

using namespace LearnVulkan;

///////////////////////////////////////////////////////
///				Vulkan Object Deleter
///////////////////////////////////////////////////////

#define DEFINE_VULKAN_OBJECT_DELETER(DEL_NAME, VAR) \
void VulkanObject::_Internal::DEL_NAME::operator()(const pointer VAR) const noexcept

DEFINE_VULKAN_OBJECT_DELETER(AllocatorDestroyer, allocator) {
	vmaDestroyAllocator(allocator);
}

DEFINE_VULKAN_OBJECT_DELETER(AllocationFreer, allocation) {
	vmaFreeMemory(this->Allocator, allocation);
}

DEFINE_VULKAN_OBJECT_DELETER(InstanceDestroyer, instance) {
	vkDestroyInstance(instance, nullptr);
}

DEFINE_VULKAN_OBJECT_DELETER(DeviceDestroyer, device) {
	vkDestroyDevice(device, nullptr);
}

DEFINE_VULKAN_OBJECT_DELETER(DebugUtilsMessengerEXTDestroyer, dbg_msg) {
	vkDestroyDebugUtilsMessengerEXT(this->Instance, dbg_msg, nullptr);
}

DEFINE_VULKAN_OBJECT_DELETER(SurfaceKHRDestroyer, surface) {
	vkDestroySurfaceKHR(this->Instance, surface, nullptr);
}

DEFINE_VULKAN_OBJECT_DELETER(SwapchainKHRDestroyer, swapchain) {
	vkDestroySwapchainKHR(this->Device, swapchain, nullptr);
}

DEFINE_VULKAN_OBJECT_DELETER(ImageDestroyer, image) {
	vkDestroyImage(this->Device, image, nullptr);
}

DEFINE_VULKAN_OBJECT_DELETER(ImageViewDestroyer, iv) {
	vkDestroyImageView(this->Device, iv, nullptr);
}

DEFINE_VULKAN_OBJECT_DELETER(ShaderModuleDestroyer, sm) {
	vkDestroyShaderModule(this->Device, sm, nullptr);
}

DEFINE_VULKAN_OBJECT_DELETER(PipelineDestroyer, pipeline) {
	vkDestroyPipeline(this->Device, pipeline, nullptr);
}

DEFINE_VULKAN_OBJECT_DELETER(PipelineLayoutDestroyer, pipeline_layout) {
	vkDestroyPipelineLayout(this->Device, pipeline_layout, nullptr);
}

DEFINE_VULKAN_OBJECT_DELETER(RenderPassDestroyer, render_pass) {
	vkDestroyRenderPass(this->Device, render_pass, nullptr);
}

DEFINE_VULKAN_OBJECT_DELETER(FramebufferDestroyer, fbo) {
	vkDestroyFramebuffer(this->Device, fbo, nullptr);
}

DEFINE_VULKAN_OBJECT_DELETER(CommandPoolDestroyer, cmd_pool) {
	vkDestroyCommandPool(this->Device, cmd_pool, nullptr);
}

DEFINE_VULKAN_OBJECT_DELETER(CommandBufferFreer, cmd_buf) {
	vkFreeCommandBuffers(this->Device, this->CmdPool, 1u, &cmd_buf);
}

DEFINE_VULKAN_OBJECT_DELETER(CommandBuffersFreer, cmd_bufs) {
	vkFreeCommandBuffers(this->Device, this->CmdPool, this->Count, cmd_bufs);
	delete[] cmd_bufs;
}

DEFINE_VULKAN_OBJECT_DELETER(SemaphoreDestroyer, semaphore) {
	vkDestroySemaphore(this->Device, semaphore, nullptr);
}

DEFINE_VULKAN_OBJECT_DELETER(BufferDestroyer, buffer) {
	vkDestroyBuffer(this->Device, buffer, nullptr);
}

DEFINE_VULKAN_OBJECT_DELETER(DescriptorSetLayoutDestroyer, des_set_layout) {
	vkDestroyDescriptorSetLayout(this->Device, des_set_layout, nullptr);
}

DEFINE_VULKAN_OBJECT_DELETER(DescriptorPoolDestroyer, des_set_pool) {
	vkDestroyDescriptorPool(this->Device, des_set_pool, nullptr);
}

DEFINE_VULKAN_OBJECT_DELETER(SamplerDestroyer, sampler) {
	vkDestroySampler(this->Device, sampler, nullptr);
}

void VulkanObject::_Internal::MemoryUnmapper::operator()(void*) const noexcept {
	vmaUnmapMemory(this->Allocator, this->Allocation);
}

DEFINE_VULKAN_OBJECT_DELETER(AccelerationStructureKHRDestroyer, as) {
	vkDestroyAccelerationStructureKHR(this->Device, as, nullptr);
}

DEFINE_VULKAN_OBJECT_DELETER(QueryPoolDestroyer, query_pool) {
	vkDestroyQueryPool(this->Device, query_pool, nullptr);
}

/////////////////////////////////////////////////////
///				Vulkan Object Creator
////////////////////////////////////////////////////

#define DEFINE_VULKAN_OBJECT_CREATOR(RET, CRE_NAME, ...) VulkanObject::RET VulkanObject::CRE_NAME(__VA_ARGS__)

void* VulkanObject::_Internal::mapAllocation(const VmaAllocator allocator, const VmaAllocation allocation) {
	void* mapped;
	CHECK_VULKAN_ERROR(vmaMapMemory(allocator, allocation, &mapped));
	return mapped;
}

DEFINE_VULKAN_OBJECT_CREATOR(Allocator, createAllocator, const VmaAllocatorCreateInfo& pCreateInfo) {
	VmaAllocator allocator;
	CHECK_VULKAN_ERROR(vmaCreateAllocator(&pCreateInfo, &allocator));
	return allocator;
}

DEFINE_VULKAN_OBJECT_CREATOR(BufferAllocation, createBufferFromAllocator, const VkDevice device,
	const VmaAllocator allocator, const VkBufferCreateInfo& pBufferCreateInfo, const VmaAllocationCreateInfo& pAllocationCreateInfo) {
	VkBuffer buffer;
	VmaAllocation allocation;
	CHECK_VULKAN_ERROR(vmaCreateBuffer(allocator, &pBufferCreateInfo, &pAllocationCreateInfo, &buffer, &allocation, nullptr));
	return make_pair(Allocation(allocation, { allocator }), Buffer(buffer, { device }));
}

DEFINE_VULKAN_OBJECT_CREATOR(ImageAllocation, createImageFromAllocator, const VkDevice device,
	const VmaAllocator allocator, const VkImageCreateInfo& pImageCreateInfo, const VmaAllocationCreateInfo& pAllocationCreateInfo) {
	VkImage image;
	VmaAllocation allocation;
	CHECK_VULKAN_ERROR(vmaCreateImage(allocator, &pImageCreateInfo, &pAllocationCreateInfo, &image, &allocation, nullptr));
	return make_pair(Allocation(allocation, { allocator }), Image(image, { device }));
}

DEFINE_VULKAN_OBJECT_CREATOR(Instance, createInstance, const VkInstanceCreateInfo& pCreateInfo) {
	VkInstance instance;
	CHECK_VULKAN_ERROR(vkCreateInstance(&pCreateInfo, nullptr, &instance));
	return instance;
}

DEFINE_VULKAN_OBJECT_CREATOR(Device, createDevice, const VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo& pCreateInfo) {
	VkDevice device;
	CHECK_VULKAN_ERROR(vkCreateDevice(physicalDevice, &pCreateInfo, nullptr, &device));
	return device;
}

DEFINE_VULKAN_OBJECT_CREATOR(ImageView, createImageView, const VkDevice device, const VkImageViewCreateInfo& pCreateInfo) {
	VkImageView image_view;
	CHECK_VULKAN_ERROR(vkCreateImageView(device, &pCreateInfo, nullptr, &image_view));
	return ImageView(image_view, { device });
}

DEFINE_VULKAN_OBJECT_CREATOR(ShaderModule, createShaderModule, const VkDevice device, const VkShaderModuleCreateInfo& pCreateInfo) {
	VkShaderModule sm;
	CHECK_VULKAN_ERROR(vkCreateShaderModule(device, &pCreateInfo, nullptr, &sm));
	return ShaderModule(sm, { device });
}

DEFINE_VULKAN_OBJECT_CREATOR(Pipeline, createGraphicsPipeline,
	const VkDevice device, const VkPipelineCache pipelineCache, const VkGraphicsPipelineCreateInfo& pCreateInfos) {
	VkPipeline pipeline;
	CHECK_VULKAN_ERROR(vkCreateGraphicsPipelines(device, pipelineCache, 1u, &pCreateInfos, nullptr, &pipeline));
	return Pipeline(pipeline, { device });
}

DEFINE_VULKAN_OBJECT_CREATOR(Pipeline, createComputePipeline, const VkDevice device, const VkPipelineCache pipelineCache,
	const VkComputePipelineCreateInfo& pCreateInfos) {
	VkPipeline pipeline;
	CHECK_VULKAN_ERROR(vkCreateComputePipelines(device, pipelineCache, 1u, &pCreateInfos, nullptr, &pipeline));
	return Pipeline(pipeline, { device });
}

DEFINE_VULKAN_OBJECT_CREATOR(PipelineLayout, createPipelineLayout, const VkDevice device, const VkPipelineLayoutCreateInfo& pCreateInfo) {
	VkPipelineLayout pipeline_layout;
	CHECK_VULKAN_ERROR(vkCreatePipelineLayout(device, &pCreateInfo, nullptr, &pipeline_layout));
	return PipelineLayout(pipeline_layout, { device });
}

DEFINE_VULKAN_OBJECT_CREATOR(RenderPass, createRenderPass2, const VkDevice device, const VkRenderPassCreateInfo2& pCreateInfo) {
	VkRenderPass render_pass;
	CHECK_VULKAN_ERROR(vkCreateRenderPass2(device, &pCreateInfo, nullptr, &render_pass));
	return RenderPass(render_pass, { device });
}

DEFINE_VULKAN_OBJECT_CREATOR(Framebuffer, createFramebuffer, const VkDevice device, const VkFramebufferCreateInfo& pCreateInfo) {
	VkFramebuffer fbo;
	CHECK_VULKAN_ERROR(vkCreateFramebuffer(device, &pCreateInfo, nullptr, &fbo));
	return Framebuffer(fbo, { device });
}

DEFINE_VULKAN_OBJECT_CREATOR(CommandPool, createCommandPool, const VkDevice device, const VkCommandPoolCreateInfo& pCreateInfo) {
	VkCommandPool cmd_pool;
	CHECK_VULKAN_ERROR(vkCreateCommandPool(device, &pCreateInfo, nullptr, &cmd_pool));
	return CommandPool(cmd_pool, { device });
}

DEFINE_VULKAN_OBJECT_CREATOR(CommandBuffer, allocateCommandBuffer, const VkDevice device, const VkCommandBufferAllocateInfo& pAllocateInfo) {
	assert(pAllocateInfo.commandBufferCount == 1u);
	VkCommandBuffer cmd;
	CHECK_VULKAN_ERROR(vkAllocateCommandBuffers(device, &pAllocateInfo, &cmd));
	return CommandBuffer(cmd, { device, pAllocateInfo.commandPool });
}

DEFINE_VULKAN_OBJECT_CREATOR(CommandBufferArray, allocateCommandBuffers, const VkDevice device, const VkCommandBufferAllocateInfo& pAllocateInfo) {
	const uint32_t count = pAllocateInfo.commandBufferCount;
	VkCommandBuffer* const raw_cmd_bufs = new VkCommandBuffer[count];
	
	try {
		CHECK_VULKAN_ERROR(vkAllocateCommandBuffers(device, &pAllocateInfo, raw_cmd_bufs));
	} catch (...) {
		delete[] raw_cmd_bufs;
		throw;
	}
	return CommandBufferArray(raw_cmd_bufs, { count, device, pAllocateInfo.commandPool });
}

DEFINE_VULKAN_OBJECT_CREATOR(Semaphore, createSemaphore, const VkDevice device, const VkSemaphoreCreateInfo& pCreateInfo) {
	VkSemaphore semaphore;
	CHECK_VULKAN_ERROR(vkCreateSemaphore(device, &pCreateInfo, nullptr, &semaphore));
	return Semaphore(semaphore, { device });
}

DEFINE_VULKAN_OBJECT_CREATOR(DescriptorSetLayout, createDescriptorSetLayout, const VkDevice device,
	const VkDescriptorSetLayoutCreateInfo& pCreateInfo) {
	VkDescriptorSetLayout des_set_layout;
	CHECK_VULKAN_ERROR(vkCreateDescriptorSetLayout(device, &pCreateInfo, nullptr, &des_set_layout));
	return DescriptorSetLayout(des_set_layout, { device });
}

DEFINE_VULKAN_OBJECT_CREATOR(DescriptorPool, createDescriptorPool, const VkDevice device, const VkDescriptorPoolCreateInfo& pCreateInfo) {
	VkDescriptorPool des_set_pool;
	CHECK_VULKAN_ERROR(vkCreateDescriptorPool(device, &pCreateInfo, nullptr, &des_set_pool));
	return DescriptorPool(des_set_pool, { device });
}

DEFINE_VULKAN_OBJECT_CREATOR(Sampler, createSampler, const VkDevice device, const VkSamplerCreateInfo& pCreateInfo) {
	VkSampler sampler;
	CHECK_VULKAN_ERROR(vkCreateSampler(device, &pCreateInfo, nullptr, &sampler));
	return Sampler(sampler, { device });
}

DEFINE_VULKAN_OBJECT_CREATOR(QueryPool, createQueryPool, const VkDevice device, const VkQueryPoolCreateInfo& pCreateInfo) {
	VkQueryPool query_pool;
	CHECK_VULKAN_ERROR(vkCreateQueryPool(device, &pCreateInfo, nullptr, &query_pool));
	return QueryPool(query_pool, { device });
}

DEFINE_VULKAN_OBJECT_CREATOR(DebugUtilsMessengerEXT, createDebugUtilsMessengerEXT,
	const VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT& pCreateInfo) {
	VkDebugUtilsMessengerEXT messenger;
	CHECK_VULKAN_ERROR(vkCreateDebugUtilsMessengerEXT(instance, &pCreateInfo, nullptr, &messenger));
	return DebugUtilsMessengerEXT(messenger, { instance });
}

DEFINE_VULKAN_OBJECT_CREATOR(SurfaceKHR, createSurfaceKHR, const VkInstance instance, const VkSurfaceKHR surface) noexcept {
	return SurfaceKHR(surface, { instance });
}

DEFINE_VULKAN_OBJECT_CREATOR(SwapchainKHR, createSwapchainKHR, const VkDevice device, const VkSwapchainCreateInfoKHR& pCreateInfo) {
	VkSwapchainKHR swapchain;
	CHECK_VULKAN_ERROR(vkCreateSwapchainKHR(device, &pCreateInfo, nullptr, &swapchain));
	return SwapchainKHR(swapchain, { device });
}

DEFINE_VULKAN_OBJECT_CREATOR(AccelerationStructureKHR, createAccelerationStructureKHR,
	const VkDevice device, const VkAccelerationStructureCreateInfoKHR& pCreateInfo) {
	VkAccelerationStructureKHR as;
	CHECK_VULKAN_ERROR(vkCreateAccelerationStructureKHR(device, &pCreateInfo, nullptr, &as));
	return AccelerationStructureKHR(as, { device });
}