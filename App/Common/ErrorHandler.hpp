#pragma once

#include <Volk/volk.h>
#include <vulkan/vk_enum_string_helper.h>

#include <source_location>

namespace LearnVulkan {

	/**
	 * @brief Safety check for API calls that returns an error code.
	*/
	namespace ErrorHandler {

		/**
		 * @brief Throw an exception as an error has been identified.
		 * @param prefix_info A string to be prepended at the start of the error message.
		 * @param src The source location.
		*/
		[[noreturn]] void throwError(const char*, const std::source_location&);

		//Check for Vulkan API return result.
		inline void checkVulkanError(const VkResult code, const std::source_location src = std::source_location::current()) {
			if (code != VkResult::VK_SUCCESS) {
				throwError(string_VkResult(code), src);
			}
		}

	}

}

#define CHECK_VULKAN_ERROR(FUNC) LearnVulkan::ErrorHandler::checkVulkanError(FUNC)