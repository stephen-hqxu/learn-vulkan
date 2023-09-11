#include "CommandBufferManager.hpp"
#include "../../Common/ErrorHandler.hpp"

#include <algorithm>

#include <stdexcept>
#include <cassert>

using std::span;
using std::ranges::transform;

using namespace LearnVulkan;
namespace VKO = VulkanObject;
namespace CmdMgr = CommandBufferManager;

#define EXPAND_COMMAND_BUFFER_SUBMIT_INFO const auto [cmd, cmd_submit_memory] = cmd_buf_submit_info
#define EXPAND_WAIT_SEMAPHORE_SUBMIT_INFO const auto [wait_op, wait_sema_submit_memory] = wait_sema_submit_info
#define EXPAND_SIGNAL_SEMAPHORE_SUBMIT_INFO const auto [signal_op, signal_sema_submit_memory] = signal_sema_submit_info

namespace {

	void fillCommandBufferSubmitInfo(const CmdMgr::_Internal:: CommandBufferSubmitInfo& cmd_buf_submit_info) {
		EXPAND_COMMAND_BUFFER_SUBMIT_INFO;
		assert(cmd.size() == cmd_submit_memory.size());

		transform(cmd, cmd_submit_memory.begin(), [](const auto cmd) constexpr noexcept {
			return VkCommandBufferSubmitInfo {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
				.commandBuffer = cmd
			};
		});
	}

	//HACK: it works with both wait and signal semaphore, use this name just to make good use of the macro
	void fillSemaphoreSubmitInfo(const CmdMgr::_Internal::SemaphoreSubmitInfo& wait_sema_submit_info) {
		EXPAND_WAIT_SEMAPHORE_SUBMIT_INFO;
		assert(wait_op.size() == wait_sema_submit_memory.size());

		transform(wait_op, wait_sema_submit_memory.begin(), [](const auto& op) constexpr noexcept {
			const auto [sema, stage, value] = op;
			return VkSemaphoreSubmitInfo {
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
				.semaphore = sema,
				.value = value,
				.stageMask = stage
			};
		});
	}

}

void CmdMgr::_Internal::submit(const CommandSubmitInfo& submit_info, const CommandBufferSubmitInfo cmd_buf_submit_info,
	const SemaphoreSubmitInfo wait_sema_submit_info, const SemaphoreSubmitInfo signal_sema_submit_info, const VkFence fence) {
	::fillCommandBufferSubmitInfo(cmd_buf_submit_info);
	::fillSemaphoreSubmitInfo(wait_sema_submit_info);
	::fillSemaphoreSubmitInfo(signal_sema_submit_info);

	EXPAND_COMMAND_BUFFER_SUBMIT_INFO;
	EXPAND_WAIT_SEMAPHORE_SUBMIT_INFO;
	EXPAND_SIGNAL_SEMAPHORE_SUBMIT_INFO;
	const VkSubmitInfo2 queue_submit_info {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
		.waitSemaphoreInfoCount = static_cast<uint32_t>(wait_sema_submit_memory.size()),
		.pWaitSemaphoreInfos = wait_sema_submit_memory.data(),
		.commandBufferInfoCount = static_cast<uint32_t>(cmd.size()),
		.pCommandBufferInfos = cmd_submit_memory.data(),
		.signalSemaphoreInfoCount = static_cast<uint32_t>(signal_sema_submit_memory.size()),
		.pSignalSemaphoreInfos = signal_sema_submit_memory.data()
	};

	const auto [device, queue] = submit_info;
	if (fence) {
		CHECK_VULKAN_ERROR(vkResetFences(device, 1u, &fence));
	}
	CHECK_VULKAN_ERROR(vkQueueSubmit2(queue, 1u, &queue_submit_info, fence));
}

VKO::CommandPool CmdMgr::createCommandPool(const VkDevice device, const VkCommandPoolCreateFlags flag, const uint32_t queue_idx) {
	const VkCommandPoolCreateInfo pool_create_info {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = flag,
		.queueFamilyIndex = queue_idx
	};
	return VKO::createCommandPool(device, pool_create_info);
}

CmdMgr::AllocatedCommandBuffer CmdMgr::allocateCommandBuffer(const VulkanContext& ctx,
	const VkCommandBufferLevel level, const CommandBufferType type) {
	VkCommandBufferAllocateInfo cmd_allocate_info {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.level = level,
		.commandBufferCount = 1u
	};

	using enum CommandBufferType;
	switch (type) {
	case Reshape:
		cmd_allocate_info.commandPool = ctx.CommandPool.Reshape;
		return VKO::allocateCommandBuffer(ctx.Device, cmd_allocate_info);
	case InFlight:
	{
		InFlightCommandBufferArray cmd;
		transform(ctx.CommandPool.InFlightCommandPool, cmd.begin(),
			[device = *ctx.Device, &cmd_allocate_info](const VkCommandPool pool) {
				cmd_allocate_info.commandPool = pool;
				return VKO::allocateCommandBuffer(device, cmd_allocate_info);
			}, [](const auto& pool_obj) constexpr noexcept { return *pool_obj; });
		return cmd;
	}
	default:
		throw std::runtime_error("The type of command buffer to be allocated is unknown.");
	}
}

void CmdMgr::beginOneTimeSubmit(const VkCommandBuffer cmd) {
	constexpr static VkCommandBufferBeginInfo one_time_begin_info {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	};
	CHECK_VULKAN_ERROR(vkBeginCommandBuffer(cmd, &one_time_begin_info));
}

void CmdMgr::beginOneTimeSubmitSecondary(const VkCommandBuffer cmd) {
	constexpr static VkCommandBufferInheritanceInfo inheritance_info {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO
	};
	constexpr static VkCommandBufferBeginInfo cmd_begin {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		.pInheritanceInfo = &inheritance_info
	};
	CHECK_VULKAN_ERROR(vkBeginCommandBuffer(cmd, &cmd_begin));
}