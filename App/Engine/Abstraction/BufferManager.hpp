#pragma once

#include "../../Common/VulkanObject.hpp"

#include <type_traits>
#include <cstdint>

namespace LearnVulkan {

	/**
	 * @brief A factory for quickly managing buffers for different usage.
	*/
	namespace BufferManager {

		/**
		 * @brief Specifies the host memory access pattern when the buffer is mappable from the host.
		*/
		enum class HostAccessPattern : std::underlying_type_t<VmaAllocationCreateFlagBits> {
			Sequential = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
			Random = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT
		};

		struct BufferCreateInfo {

			VkDevice Device;
			VmaAllocator Allocator;
			size_t Size;/**< In byte */

		};

		/**
		 * @brief Get the device address of a buffer.
		 * @param device The device.
		 * @param buffer The buffer whose device address to be retrieved.
		 * @return The device address of the buffer.
		*/
		VkDeviceAddress addressOf(VkDevice, VkBuffer) noexcept;

		/**
		 * @brief Create an allocated staging buffer for memory transfer.
		 * This function is an alias of `createTransientHostBuffer`.
		 * @param create_info The buffer creation info.
		 * @param access The host access pattern.
		 * @return The staging buffer.
		*/
		VulkanObject::BufferAllocation createStagingBuffer(const BufferCreateInfo&, HostAccessPattern);

		/**
		 * @brief Create a buffer with host-visible memory, and the memory is suitable for temporary use.
		 * The created staging buffer is non-coherent and requires manual flushing.
		 * @param create_info The buffer creation info.
		 * @param usage The usage of the buffer.
		 * @param access The host access pattern.
		 * @return The transient host buffer.
		*/
		VulkanObject::BufferAllocation createTransientHostBuffer(const BufferCreateInfo&, VkBufferUsageFlags, HostAccessPattern);

		/**
		 * @brief Create a buffer with device-local memory location.
		 * @param create_info The buffer creation info.
		 * @param usage The usage of the buffer.
		 * @return The device buffer.
		*/
		VulkanObject::BufferAllocation createDeviceBuffer(const BufferCreateInfo&, VkBufferUsageFlags);

		/**
		 * @brief Create a buffer that is used for sharing a SSBO to all shaders, such as camera buffer.
		 * This buffer will be persistently mappable by the host and uncached, thus not ideal for host read back.
		 * The buffer whose device address may be fetched.
		 * @param create_info The buffer create info.
		 * @param access The host access pattern.
		 * @return The allocated global storage buffer.
		*/
		VulkanObject::BufferAllocation createGlobalStorageBuffer(const BufferCreateInfo&, HostAccessPattern);

		/**
		 * @brief Create a buffer used as a descriptor buffer.
		 * @param create_info The buffer create info.
		 * @param usage The usage of the buffer.
		 * A device address usage is automatically included.
		 * @return The allocated descriptor buffer.
		*/
		VulkanObject::BufferAllocation createDescriptorBuffer(const BufferCreateInfo&, VkBufferUsageFlags);

		/**
		 * @brief Record commands to copy between two buffers from the beginning of two buffers.
		 * @param source The copy source.
		 * @param destination The copy destination.
		 * The destination buffer must have at least as much space as the source.
		 * @param cmd The command buffer where the commands are recorded.
		 * @param size The number of byte to be copied.
		*/
		void recordCopyBuffer(VkBuffer, VkBuffer, VkCommandBuffer, size_t);

	}

}