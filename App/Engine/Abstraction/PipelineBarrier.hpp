#pragma once

#include "../../Common/FixedArray.hpp"

#include <Volk/volk.h>

#define EXPAND_BARRIER const auto [src_stage, src_access, dst_stage, dst_access] = barrier
#define EXPAND_QUEUE_FAMILY const auto [src_queue, dst_queue] = queue_family

namespace LearnVulkan {

	/**
	 * @brief Information structures for pipeline barrier.
	*/
	namespace PipelineBarrierInfo {
	
		struct BarrierInfo {

			VkPipelineStageFlags2 SourceStage;
			VkAccessFlags2 SourceAccess;

			VkPipelineStageFlags2 TargetStage;
			VkAccessFlags2 TargetAccess;

		};

		struct ImageLayoutTransitionInfo {

			VkImageLayout OldLayout, NewLayout;

		};

		struct QueueFamilyTransitionInfo {

			uint32_t Source = VK_QUEUE_FAMILY_IGNORED,
				Target = VK_QUEUE_FAMILY_IGNORED;

		};

	}
	
	/**
	 * @brief A helper for calling pipeline barrier with minimal hassle.
	 * @tparam NMemBar, NBufBar, NImgBar The number of memory barrier, buffer barrier and image barrier, respectively.
	*/
	template<size_t NMemBar, size_t NBufBar, size_t NImgBar>
	class PipelineBarrier {
	public:

		FixedArray<VkMemoryBarrier2, NMemBar> MemoryBarrier;
		FixedArray<VkBufferMemoryBarrier2, NBufBar> BufferBarrier;
		FixedArray<VkImageMemoryBarrier2, NImgBar> ImageBarrier;

		constexpr PipelineBarrier() noexcept = default;

		~PipelineBarrier() = default;

		//Add a memory barrier.
		constexpr void addMemoryBarrier(const PipelineBarrierInfo::BarrierInfo& barrier) noexcept {
			EXPAND_BARRIER;

			this->MemoryBarrier.pushBack(VkMemoryBarrier2 {
				.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
				.srcStageMask = src_stage,
				.srcAccessMask = src_access,
				.dstStageMask = dst_stage,
				.dstAccessMask = dst_access
			});
		}

		//Add a buffer barrier with full information.
		constexpr void addBufferBarrier(const PipelineBarrierInfo::BarrierInfo& barrier,
			const PipelineBarrierInfo::QueueFamilyTransitionInfo& queue_family,
			const VkBuffer buffer, const VkDeviceSize offset, const VkDeviceSize size) noexcept {
			EXPAND_BARRIER;
			EXPAND_QUEUE_FAMILY;

			this->BufferBarrier.pushBack(VkBufferMemoryBarrier2 {
				.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
				.srcStageMask = src_stage,
				.srcAccessMask = src_access,
				.dstStageMask = dst_stage,
				.dstAccessMask = dst_access,
				.srcQueueFamilyIndex = src_queue,
				.dstQueueFamilyIndex = dst_queue,
				.buffer = buffer,
				.offset = offset,
				.size = size
			});
		}

		//Add a buffer barrier with no queue family transition, and barrier on the whole buffer size with zero offset.
		constexpr void addBufferBarrier(const PipelineBarrierInfo::BarrierInfo& barrier, const VkBuffer buffer) noexcept {
			this->addBufferBarrier(barrier, { }, buffer, { }, VK_WHOLE_SIZE);
		}

		//Add an image barrier with full information.
		constexpr void addImageBarrier(const PipelineBarrierInfo::BarrierInfo& barrier,
			const PipelineBarrierInfo::ImageLayoutTransitionInfo& layout,
			const PipelineBarrierInfo::QueueFamilyTransitionInfo& queue_family,
			const VkImage image, const VkImageSubresourceRange& sub_res_range) noexcept {
			EXPAND_BARRIER;
			const auto [old_layout, new_layout] = layout;
			EXPAND_QUEUE_FAMILY;

			this->ImageBarrier.pushBack(VkImageMemoryBarrier2 {
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.srcStageMask = src_stage,
				.srcAccessMask = src_access,
				.dstStageMask = dst_stage,
				.dstAccessMask = dst_access,
				.oldLayout = old_layout,
				.newLayout = new_layout,
				.srcQueueFamilyIndex = src_queue,
				.dstQueueFamilyIndex = dst_queue,
				.image = image,
				.subresourceRange = sub_res_range
			});
		}

		//Add an image barrier that doesn't care about queue family transition.
		constexpr void addImageBarrier(const PipelineBarrierInfo::BarrierInfo& barrier,
			const PipelineBarrierInfo::ImageLayoutTransitionInfo& layout,
			const VkImage image, const VkImageSubresourceRange& sub_res_range) noexcept {
			this->addImageBarrier(barrier, layout, { }, image, sub_res_range);
		}

		/**
		 * @brief Record the pipeline barrier into the command buffer.
		 * @param cmd The command buffer where the pipeline barrier command is placed.
		 * @param dep_flag Specifies any dependency flag.
		*/
		void record(const VkCommandBuffer cmd, const VkDependencyFlags dep_flag = { }) const noexcept {
			const VkDependencyInfo dep {
				.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
				.dependencyFlags = dep_flag,
				.memoryBarrierCount = static_cast<uint32_t>(this->MemoryBarrier.size()),
				.pMemoryBarriers = this->MemoryBarrier.data(),
				.bufferMemoryBarrierCount = static_cast<uint32_t>(this->BufferBarrier.size()),
				.pBufferMemoryBarriers = this->BufferBarrier.data(),
				.imageMemoryBarrierCount = static_cast<uint32_t>(this->ImageBarrier.size()),
				.pImageMemoryBarriers = this->ImageBarrier.data()
			};
			vkCmdPipelineBarrier2(cmd, &dep);
		}

		/**
		 * @brief Clear all previously added pipeline barriers.
		*/
		constexpr void clear() noexcept {
			this->MemoryBarrier.clear();
			this->BufferBarrier.clear();
			this->ImageBarrier.clear();
		}
	
	};

}

#undef EXPAND_BARRIER
#undef EXPAND_QUEUE_FAMILY