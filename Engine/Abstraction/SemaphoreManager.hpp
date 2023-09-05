#pragma once

#include "../../Common/VulkanObject.hpp"

#include <array>
#include <span>

#include <utility>
#include <algorithm>
#include <ranges>
#include <limits>

#include <cstdint>

namespace LearnVulkan {

	/**
	 * @brief A manager to quickly create and manipulate semaphore.
	*/
	namespace SemaphoreManager {

		using SemaphoreWaitInfo = std::pair<VkSemaphore, uint64_t>;/**< Semaphore and timeline value. */

		namespace _Internal {
		
			//Delegated from the template version of `wait` function.
			void wait(VkDevice, VkSemaphoreWaitFlags, std::span<const VkSemaphore>, std::span<const uint64_t>, uint64_t);

		}

		/**
		 * @brief Create a binary semaphore.
		 * @param device The device.
		 * @return Created binary semaphore.
		*/
		VulkanObject::Semaphore createBinarySemaphore(VkDevice);

		/**
		 * @brief Create a timeline semaphore.
		 * @param device The device.
		 * @param init_value The value to initialise the timeline semaphore with.
		 * @return Created timeline semaphore.
		*/
		VulkanObject::Semaphore createTimelineSemaphore(VkDevice, uint64_t);

		/**
		 * @brief Wait for a number of semaphores from host.
		 * @tparam Count The number of semaphore to wait.
		 * @param device The device.
		 * @param flag Specify any flag.
		 * @param wait_info The wait information.
		 * @param timeout Specify the timeout, default to unlimited timeout.
		*/
		template<size_t Count>
		inline void wait(const VkDevice device, const VkSemaphoreWaitFlags flag,
			const std::array<SemaphoreWaitInfo, Count> wait_info, const uint64_t timeout = std::numeric_limits<uint64_t>::max()) {
			using std::array, std::ranges::copy, std::ranges::views::elements;
			//transform from AoS to SoA
			array<VkSemaphore, Count> sema;
			array<uint64_t, Count> value;

			copy(elements<0u>(wait_info), sema.begin());
			copy(elements<1u>(wait_info), value.begin());
			_Internal::wait(device, flag, sema, value, timeout);
		}

	}

}