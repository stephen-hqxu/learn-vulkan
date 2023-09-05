#include "BufferManager.hpp"
#include "../../Common/ErrorHandler.hpp"

#include <stdexcept>

#include <cstring>

using std::runtime_error;

using namespace LearnVulkan;
namespace VKO = VulkanObject;

namespace {

	inline constexpr VmaAllocationCreateFlags convertHostAccessFlag(const BufferManager::HostAccessPattern access) noexcept {
		return static_cast<VmaAllocationCreateFlags>(access);
	}

	inline VkBufferCreateInfo createCommonBufferInfo(const size_t size, const VkBufferUsageFlags usage) noexcept {
		return {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = static_cast<VkDeviceSize>(size),
			.usage = usage,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE
		};
	}

}

#define EXPAND_BUFFER_INFO const auto [device, allocator, size] = create_info

VkDeviceAddress BufferManager::addressOf(const VkDevice device, const VkBuffer buffer) noexcept {
	const VkBufferDeviceAddressInfo addr_info {
		.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
		.buffer = buffer
	};
	return vkGetBufferDeviceAddress(device, &addr_info);
}

VKO::BufferAllocation BufferManager::createStagingBuffer(const BufferCreateInfo& create_info, const HostAccessPattern access) {
	return BufferManager::createTransientHostBuffer(create_info, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, access);
}

VKO::BufferAllocation BufferManager::createTransientHostBuffer(const BufferCreateInfo& create_info,
	const VkBufferUsageFlags usage, const HostAccessPattern access) {
	EXPAND_BUFFER_INFO;

	const VmaAllocationCreateInfo staging_mem_info {
		.flags = ::convertHostAccessFlag(access) | VMA_ALLOCATION_CREATE_STRATEGY_MIN_TIME_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
		.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
	};
	return VKO::createBufferFromAllocator(device, allocator, ::createCommonBufferInfo(size, usage), staging_mem_info);
}

VKO::BufferAllocation BufferManager::createDeviceBuffer(const BufferCreateInfo& create_info, const VkBufferUsageFlags usage) {
	EXPAND_BUFFER_INFO;

	constexpr static VmaAllocationCreateInfo deviceLocalAllocationCreateInfo = {
		.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
		.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	};
	return VKO::createBufferFromAllocator(device, allocator, ::createCommonBufferInfo(size, usage), deviceLocalAllocationCreateInfo);
}

VKO::BufferAllocation BufferManager::createGlobalStorageBuffer(const BufferCreateInfo& create_info, const HostAccessPattern access) {
	EXPAND_BUFFER_INFO;

	const VmaAllocationCreateInfo ssbo_mem_info {
		.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | ::convertHostAccessFlag(access) | VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
		.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
		.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	};
	return VKO::createBufferFromAllocator(device, allocator,
		::createCommonBufferInfo(size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT), ssbo_mem_info);
}

VKO::BufferAllocation BufferManager::createDescriptorBuffer(const BufferCreateInfo& create_info, const VkBufferUsageFlags usage) {
	EXPAND_BUFFER_INFO;

	constexpr static VmaAllocationCreateInfo descriptorBufferAllocationCreateInfo = {
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
		.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
		.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	};
	return VKO::createBufferFromAllocator(device, allocator,
		::createCommonBufferInfo(size, usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT), descriptorBufferAllocationCreateInfo);
}

void BufferManager::recordCopyBuffer(const VkBuffer source, const VkBuffer destination,
	const VkCommandBuffer cmd, const size_t size) {
	const VkBufferCopy2 region {
		.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
		.size = size
	};
	const VkCopyBufferInfo2 copy_info {
		.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
		.srcBuffer = source,
		.dstBuffer = destination,
		.regionCount = 1u,
		.pRegions = &region
	};
	vkCmdCopyBuffer2(cmd, &copy_info);
}