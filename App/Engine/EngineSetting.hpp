#pragma once

#ifndef NDEBUG
#define LEARN_VULKAN_ENABLE_VALIDATION
#endif

namespace LearnVulkan {

	/**
	 * @brief Setting for our Vulkan renderer.
	*/
	namespace EngineSetting {

		//Whether Vulkan validation layer is enabled.
#ifdef LEARN_VULKAN_ENABLE_VALIDATION
		constexpr bool EnableValidation = true;
#else
		constexpr bool EnableValidation = false;
#endif

		/**
		 * @brief Specify the maximum number of frame that can be submitted to the queue before rendering.
		*/
		constexpr inline unsigned int MaxFrameInFlight = 2u;

	}

}