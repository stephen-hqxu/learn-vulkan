#pragma once

#include "../../Common/VulkanObject.hpp"
#include "../EngineSetting.hpp"
#include "../VulkanContext.hpp"

#include <array>
#include <span>
#include <variant>
#include <utility>

namespace LearnVulkan {

	/**
	 * @brief A factory for managing command buffer usage.
	*/
	namespace CommandBufferManager {

		/**
		 * @brief Allocated command buffer, one from each in-flight command pool.
		*/
		using InFlightCommandBufferArray = std::array<VulkanObject::CommandBuffer, EngineSetting::MaxFrameInFlight>;

		using AllocatedCommandBuffer = std::variant<VulkanObject::CommandBuffer, InFlightCommandBufferArray>;

		/**
		 * @brief The type of command buffer.
		*/
		enum class CommandBufferType : uint8_t {
			//Returns a single command buffer.
			Reshape = 0x00u,
			//Returns in-flight command buffer array.
			InFlight = 0x10u
		};

		/**
		 * @brief Information required to submit command buffers.
		*/
		struct CommandSubmitInfo {
		
			VkDevice Device;
			VkQueue Queue;
		
		};

		/**
		 * @brief Specifies use of semaphore and its stage of effectiveness.
		*/
		struct SemaphoreOperation {

			VkSemaphore Semaphore;
			VkPipelineStageFlags2 Stage;
			uint64_t Value = 0ull;/**< Only used by timeline semaphore. */

		};

		namespace _Internal {

			//Represent a data to be submitted and its allocated memory.
			template<class T, class TMem>
			using PreallocatedSubmitInfo = std::pair<
				const std::span<const T>,
				const std::span<TMem>
			>;

			using CommandBufferSubmitInfo = PreallocatedSubmitInfo<VkCommandBuffer, VkCommandBufferSubmitInfo>;
			using SemaphoreSubmitInfo = PreallocatedSubmitInfo<SemaphoreOperation, VkSemaphoreSubmitInfo>;
		
			//Delegated from the submit function.
			//The additional submit info spans for command buffer and semaphore are pre-allocated memory.
			//The content within will be overwritten.
			void submit(const CommandSubmitInfo&, CommandBufferSubmitInfo,
				SemaphoreSubmitInfo, SemaphoreSubmitInfo, VkFence);

		}

		/**
		 * @brief Create a command pool.
		 * @param device The device.
		 * @param flag The command pool create flag.
		 * @param queue_idx The queue family index.
		 * @return The created command pool.
		*/
		VulkanObject::CommandPool createCommandPool(VkDevice, VkCommandPoolCreateFlags, uint32_t);

		/**
		 * @brief Allocate command buffer.
		 * @param ctx The vulkan context.
		 * @param level The level of command buffer to be allocated.
		 * @param type The type of command buffer to be allocated.
		 * @return A variant of command buffer in different data structure, depending on type of allocation.
		*/
		AllocatedCommandBuffer allocateCommandBuffer(const VulkanContext&, VkCommandBufferLevel, CommandBufferType);
	
		/**
		 * @brief Begin a one time submission buffer.
		 * @param cmd The command buffer to begin.
		*/
		void beginOneTimeSubmit(VkCommandBuffer);

		/**
		 * @brief Similarly, but for secondary command buffer, with no inheritance info.
		 * @param cmd The command buffer to begin.
		*/
		void beginOneTimeSubmitSecondary(VkCommandBuffer);

		/**
		 * @brief Submit the command buffers to a queue.
		 * @tparam NCmd, NWait, NSignal The number of command buffer, wait semaphore and signal semaphore to be submitted.
		 * @param submit_info The submission info.
		 * @param cmd The command buffers to be submitted.
		 * @param wait The waiting semaphores.
		 * @param signal The signalling semaphores.
		 * @param fence Optional fence for sync.
		 * The fence will be reset automatically prior to submission.
		*/
		template<size_t NCmd, size_t NWait = 0u, size_t NSignal = 0u>
		inline void submit(const CommandSubmitInfo& submit_info,
			const std::array<const VkCommandBuffer, NCmd>& cmd,
			const std::array<const SemaphoreOperation, NWait>& wait,
			const std::array<const SemaphoreOperation, NSignal>& signal, const VkFence fence = VK_NULL_HANDLE) {
			using std::array;
			array<VkCommandBufferSubmitInfo, NCmd> cmd_submit_memory;
			array<VkSemaphoreSubmitInfo, NWait> wait_sema_submit_memory;
			array<VkSemaphoreSubmitInfo, NSignal> signal_sema_submit_memory;

			_Internal::submit(submit_info,
				{ cmd, cmd_submit_memory },
				{ wait, wait_sema_submit_memory },
				{ signal, signal_sema_submit_memory }, fence);
		}
	
	}

}