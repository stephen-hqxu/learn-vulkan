#pragma once

#include <Volk/volk.h>

namespace LearnVulkan {

	/**
	 * @brief A light-weight interface to give renderers access to camera functions.
	*/
	class CameraInterface {
	public:

		constexpr CameraInterface() noexcept = default;

		virtual ~CameraInterface() = default;

		/**
		 * @brief Get the camera descriptor set layout.
		 * @return The camera descriptor set layout.
		*/
		virtual VkDescriptorSetLayout descriptorSetLayout() const noexcept = 0;

		/**
		 * @brief Get the camera descriptor buffer binding info.
		 * @return The camera descriptor buffer binding info.
		*/
		virtual VkDescriptorBufferBindingInfoEXT descriptorBufferBindingInfo() const noexcept = 0;

		/**
		 * @brief Get the offset into the camera descriptor buffer given the current in-flight frame index.
		 * @param index The frame index.
		 * @return The current descriptor buffer offset in byte.
		*/
		virtual VkDeviceSize descriptorBufferOffset(unsigned int) const noexcept = 0;

	};

}