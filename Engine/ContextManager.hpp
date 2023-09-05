#pragma once

#include "EngineSetting.hpp"
#include "../Common/StaticArray.hpp"
#include "../Common/VulkanObject.hpp"

#include <memory>

#include <span>
#include <tuple>
#include <utility>

#include <cstdint>

namespace LearnVulkan {

	/**
	 * @brief Some help functions to make programming a Vulkan application easier.
	*/
	namespace ContextManager {

		/**
		 * @brief An array of extension names that are required.
		*/
		using ExtensionName = std::span<const char* const>;
		/**
		 * @brief Contains all physical device properties from Vulkan 1.0 to 1.3.
		 * They are allocated on heap so it is cheaper to move them around, also preserves the pNext pointer.
		*/
		using DeviceProperty = std::unique_ptr<std::tuple<
			VkPhysicalDeviceProperties2,
			VkPhysicalDeviceVulkan11Properties,
			VkPhysicalDeviceVulkan12Properties,
			VkPhysicalDeviceVulkan13Properties,
			VkPhysicalDeviceDescriptorBufferPropertiesEXT
		>>;

		/**
		 * @brief Image and its view for each swap chain image.
		*/
		using SwapchainImage = StaticArray<std::pair<VkImage, VulkanObject::ImageView>>;

		/**
		 * @brief A list of requirements used for selecting a capable physical device.
		*/
		struct DeviceRequirement {

			VkPhysicalDeviceType DeviceType;
			ExtensionName DeviceExtension;
			VkQueueFlags QueueFamilyCapability;/**< Can be a combination of multiple queue flag bits. */

			VkFormat Format;
			VkColorSpaceKHR ColourSpace;
			VkPresentModeKHR PresentMode;

		};

		/**
		 * @brief The return result containing the verification of layer.
		*/
		struct LayerContext {

			StaticArray<VkLayerProperties> Layer;/**< All supported layers. */

		};

		/**
		 * @brief The return result containing the selected physical device.
		*/
		struct VulkanContext {

			uint32_t TotalPhysicalDevice;/**< Number of physical devices in the system. */
			uint32_t TotalQueueFamily;/**< Number of queue family of the selected device. */

			//Information regarding the selected physical device, useful for logging.
			DeviceProperty DeviceProperty;

			//The context for the application.
			VkPhysicalDevice PhysicalDevice;
			uint32_t RenderingQueueFamily, PresentingQueueFamily;/**< Index into the queue family for rendering and presentation. */

		};

		/**
		 * @brief Verify if all requested layers are supported.
		 * @param required_layer All requested layers.
		 * @return The layer context.
		 * @exception If layer verification fails.
		*/
		LayerContext verifyLayerSupport(const ExtensionName&);

		/**
		 * @brief Select the most suitable and capable physical device from the system based on a number of criterion.
		 * @param instance The instance of the application.
		 * @param surface The surface for rendering.
		 * @param requirement The device requirement for selection.
		 * @exception If no physical device can be selected.
		*/
		VulkanContext selectPhysicalDevice(VkInstance, VkSurfaceKHR, const DeviceRequirement&);

		/**
		 * @brief Query the image from a swap chain object.
		 * @param device The device.
		 * @param sc The swap chain to be queried from.
		 * @param format The format of the image view to be created.
		 * @return The queried array of swap chain image.
		*/
		SwapchainImage querySwapchainImage(const VkDevice, const VkSwapchainKHR, VkFormat);

	}

}