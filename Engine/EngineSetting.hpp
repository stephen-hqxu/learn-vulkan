#pragma once

#include <Volk/volk.h>

#include <string_view>

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
		 * @brief Where all shader source code reside.
		*/
		constexpr inline std::string_view ShaderRoot = "C:/Users/steph/source/repos/LearnVulkan/Shader";

		/**
		 * @brief Where assets such as texture reside.
		*/
		constexpr inline std::string_view ResourceRoot = "C:\\Users\\steph\\source\\repos\\Texture";

		/**
		 * @brief Specify the maximum number of frame that can be submitted to the queue before rendering.
		*/
		constexpr inline unsigned int MaxFrameInFlight = 2u;

	}

}