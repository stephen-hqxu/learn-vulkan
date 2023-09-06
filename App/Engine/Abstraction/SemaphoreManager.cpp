#include "SemaphoreManager.hpp"
#include "../../Common/ErrorHandler.hpp"

#include <cassert>

using std::span;

using namespace LearnVulkan;
namespace VKO = VulkanObject;

void SemaphoreManager::_Internal::wait(const VkDevice device, const VkSemaphoreWaitFlags flag,
	const span<const VkSemaphore> sema, const span<const uint64_t> value, const uint64_t timeout) {
	assert(sema.size() == value.size());
	const VkSemaphoreWaitInfo wait_info {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
		.flags = flag,
		.semaphoreCount = static_cast<uint32_t>(sema.size()),
		.pSemaphores = sema.data(),
		.pValues = value.data()
	};
	CHECK_VULKAN_ERROR(vkWaitSemaphores(device, &wait_info, timeout));
}

VKO::Semaphore SemaphoreManager::createBinarySemaphore(const VkDevice device) {
	constexpr static VkSemaphoreTypeCreateInfo bin_sema {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
		.semaphoreType = VK_SEMAPHORE_TYPE_BINARY
	};
	constexpr static VkSemaphoreCreateInfo sema {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		.pNext = &bin_sema
	};
	return VKO::createSemaphore(device, sema);
}

VKO::Semaphore SemaphoreManager::createTimelineSemaphore(const VkDevice device, const uint64_t init_value) {
	const VkSemaphoreTypeCreateInfo tl_sema {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
		.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
		.initialValue = init_value
	};
	const VkSemaphoreCreateInfo sema {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		.pNext = &tl_sema
	};
	return VKO::createSemaphore(device, sema);
}