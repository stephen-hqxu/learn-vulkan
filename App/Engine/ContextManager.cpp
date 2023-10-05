#include "ContextManager.hpp"

#include "Abstraction/ImageManager.hpp"

#include "../Common/ErrorHandler.hpp"

#include <iterator>
#include <utility>

#include <algorithm>
#include <ranges>

#include <optional>
#include <string>

#include <stdexcept>
#include <cassert>

using std::make_unique;

using std::optional, std::nullopt;
using std::span;

using std::distance;

using std::ranges::find, std::ranges::find_if, std::ranges::includes, std::ranges::sort, std::ranges::is_sorted,
	std::ranges::transform, std::ranges::copy, std::ranges::views::iota;

using std::runtime_error;
using std::move;

using namespace LearnVulkan;
using ContextManager::ExtensionName,
	ContextManager::DeviceProperty;
namespace VKO = VulkanObject;

namespace {

	//Returns `s1 < s2`
	template<size_t Count = VK_MAX_EXTENSION_NAME_SIZE>
	inline constexpr bool stringLessThan(const char* const s1, const char* const s2) {
		return std::char_traits<char>::compare(s1, s2, Count) < 0;
	}

	//Returns `s1 == s2`
	template<size_t Count = VK_MAX_EXTENSION_NAME_SIZE>
	inline constexpr bool stringEqual(const char* const s1, const char* const s2) {
		return std::char_traits<char>::compare(s1, s2, Count) == 0;
	}

	//Get all layers supported by the current application. Empty array if no layer is supported.
	//The output layer properties are sorted based on layer name.
	StaticArray<VkLayerProperties> getInstanceLayer() {
		uint32_t layer_count;
		CHECK_VULKAN_ERROR(vkEnumerateInstanceLayerProperties(&layer_count, nullptr));
		if (layer_count == 0u) {
			return { };
		}

		//allocate memory to hold all layer info
		StaticArray<VkLayerProperties> layer(layer_count);
		CHECK_VULKAN_ERROR(vkEnumerateInstanceLayerProperties(&layer_count, layer.data()));
		sort(layer.toSpan(), ::stringLessThan<>,
			[](const auto& layer_props) constexpr noexcept { return layer_props.layerName; });
		return layer;
	}

	//Check if the supported layers meet the requirement.
	//*layer* range must be sorted by layer extension name.
	inline bool isLayerSuitable(const span<const VkLayerProperties> layer, const ExtensionName& required_layer) {
		constexpr static auto layer_name_projector = [](const VkLayerProperties& p) constexpr noexcept -> const char* {
			return p.layerName;
		};
		//`includes` function requires both ranges to be sorted
		auto req_layer_arr = StaticArray<const char*>(required_layer.size());
		copy(required_layer, req_layer_arr.data());

		assert(is_sorted(layer, ::stringLessThan<>, layer_name_projector));
		return includes(layer, req_layer_arr.toSpan(), ::stringEqual<>, layer_name_projector);
	}

	//Get all available physical devices on the system. Generate exception If no physical device is found.
	StaticArray<VkPhysicalDevice> getAllPhysicalDevice(const VkInstance instance) {
		uint32_t device_count;
		CHECK_VULKAN_ERROR(vkEnumeratePhysicalDevices(instance, &device_count, nullptr));
		if (device_count == 0u) {
			throw runtime_error("No available physical GPU is found to be usable by Vulkan.");
		}

		StaticArray<VkPhysicalDevice> device(device_count);
		CHECK_VULKAN_ERROR(vkEnumeratePhysicalDevices(instance, &device_count, device.data()));
		return device;
	}

	//As its name suggests...
	DeviceProperty getPhysicalDeviceProperty(const VkPhysicalDevice device) {
		auto property = make_unique<DeviceProperty::element_type>();
		auto& [dev10, dev11, dev12, dev13, descriptor_buf] = *property;

		descriptor_buf = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT
		};
		dev13 = {
			.sType = VkStructureType::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES,
			.pNext = &descriptor_buf
		};
		dev12 = {
			.sType = VkStructureType::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES,
			.pNext = &dev13
		};
		dev11 = {
			.sType = VkStructureType::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES,
			.pNext = &dev12
		};
		dev10 = {
			.sType = VkStructureType::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
			.pNext = &dev11
		};
		vkGetPhysicalDeviceProperties2(device, &dev10);
		return property;
	}

	//Get all queue families from a physical device. Return none if no queue family is found for such device.
	StaticArray<VkQueueFamilyProperties> getDeviceQueueFamily(const VkPhysicalDevice device) {
		uint32_t qf_count;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &qf_count, nullptr);
		if (qf_count == 0u) {
			return { };
		}

		StaticArray<VkQueueFamilyProperties> qf(qf_count);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &qf_count, qf.data());
		return qf;
	}

	//Get all supported extensions of a given physical device. Return none if no extension is found.
	//The output extension properties are sorted based on extension name.
	StaticArray<VkExtensionProperties> getDeviceExtension(const VkPhysicalDevice device) {
		uint32_t extension_count;
		CHECK_VULKAN_ERROR(vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr));
		if (extension_count == 0u) {
			return { };
		}

		StaticArray<VkExtensionProperties> extension(extension_count);
		CHECK_VULKAN_ERROR(vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, extension.data()));
		sort(extension.toSpan(), ::stringLessThan<>,
			[](const auto& ext_props) constexpr noexcept { return ext_props.extensionName; });
		return extension;
	}

	//Get surface format, or empty array if none found.
	StaticArray<VkSurfaceFormatKHR> getSurfaceFormat(const VkPhysicalDevice device, const VkSurfaceKHR surface) {
		uint32_t surface_format_count;
		CHECK_VULKAN_ERROR(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &surface_format_count, nullptr));
		if (surface_format_count == 0u) {
			return { };
		}

		StaticArray<VkSurfaceFormatKHR> surface_format(surface_format_count);
		CHECK_VULKAN_ERROR(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &surface_format_count, surface_format.data()));
		return surface_format;
	}

	//Get surface present mode, or empty if none found.
	StaticArray<VkPresentModeKHR> getSurfacePresentMode(const VkPhysicalDevice device, const VkSurfaceKHR surface) {
		uint32_t surface_present_count;
		CHECK_VULKAN_ERROR(vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &surface_present_count, nullptr));
		if (surface_present_count == 0u) {
			return { };
		}

		StaticArray<VkPresentModeKHR> surface_present(surface_present_count);
		CHECK_VULKAN_ERROR(vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &surface_present_count, surface_present.data()));
		return surface_present;
	}

	//Check if the given device meets the requirement.
	//*all_extensions* must be sorted by device extension name.
	bool isPhysicalDeviceSuitable(const VkPhysicalDeviceProperties dev_props, const span<const VkExtensionProperties> all_extensions,
		const VkPhysicalDeviceType required_dev_type, const ExtensionName& required_dev_ext) {
		constexpr static auto extension_name_projector = [](const VkExtensionProperties& props) constexpr noexcept -> const char* {
			return props.extensionName;
		};

		//device type check
		if (dev_props.deviceType != required_dev_type) {
			return false;
		}

		auto req_dev_ext_arr = StaticArray<const char*>(required_dev_ext.size());
		copy(required_dev_ext, req_dev_ext_arr.data());
		
		//device extension check
		assert(is_sorted(all_extensions, ::stringLessThan<>, extension_name_projector));
		return includes(all_extensions, req_dev_ext_arr.toSpan(), ::stringEqual<>, extension_name_projector);
	}

	//Check if the surface format that meets our requirement.
	inline bool isSurfaceFormatSuitable(const span<const VkSurfaceFormatKHR> surface_format,
		const VkFormat format, const VkColorSpaceKHR colour_space) {
		return find_if(surface_format,
			[format, colour_space](const VkSurfaceFormatKHR& sf)
			{ return sf.format == format && sf.colorSpace == colour_space; }) != surface_format.end();
	}

	//Check if the present mode is listed in the supported present mode.
	inline bool isPresentModeSuitable(const span<const VkPresentModeKHR> present_mode, const VkPresentModeKHR present) {
		return find(present_mode, present) != present_mode.end();
	}

	//Find a queue family for rendering/GPGPU computing. Return the index into the input array.
	//Provide the requested flag a queue should have.
	//Return none if no suitable queue family is found.
	optional<uint32_t> findRenderingQueueFamily(const span<const VkQueueFamilyProperties> all_qf, const VkQueueFlags expected_flag) {
		const auto rendering_it = find_if(all_qf, [expected_flag](const VkQueueFlags queue_flags) constexpr noexcept {
			return (queue_flags & expected_flag) == expected_flag;
		}, [](const VkQueueFamilyProperties& props) constexpr noexcept { return props.queueFlags; });

		if (rendering_it == all_qf.end()) {
			return nullopt;
		} else {
			return static_cast<uint32_t>(distance(all_qf.begin(), rendering_it));
		}
	}

	//Similarly, find the index to the queue family for presentation.
	optional<uint32_t> findPresentingQueueFamily(const VkPhysicalDevice device, const VkSurfaceKHR surface, const uint32_t qf_size) {
		const auto queue_index = iota(0u, qf_size);
		const auto presenting_it = find_if(queue_index, [device, surface](const auto i) {
			VkBool32 supported;
			CHECK_VULKAN_ERROR(vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &supported));
			return supported;
		});

		if (presenting_it == queue_index.end()) {
			return nullopt;
		} else {
			return static_cast<uint32_t>(distance(queue_index.begin(), presenting_it));
		}
	}

}

ContextManager::LayerContext ContextManager::verifyLayerSupport(const ExtensionName& required_layer) {
	StaticArray<VkLayerProperties> layer = getInstanceLayer();
	if (layer.size() == 0u || !isLayerSuitable(layer.toSpan(), required_layer)) {
		throw runtime_error("Some Layers are requested, but they are not supported by the application.");
	}

	return LayerContext {
		.Layer = move(layer)
	};
}

ContextManager::VulkanContext ContextManager::selectPhysicalDevice(
	const VkInstance instance, const VkSurfaceKHR surface, const DeviceRequirement& requirement) {
	const StaticArray<VkPhysicalDevice> device = getAllPhysicalDevice(instance);
	const span<const VkPhysicalDevice> device_span = device.toSpan();
	
	//loop through every physical device and check for their validity
	for (const auto d : device_span) {
		///////////////////////////
		///	Get stuff
		///////////////////////////
		const StaticArray<VkQueueFamilyProperties> qf = getDeviceQueueFamily(d);
		if (qf.size() == 0u) {
			continue;
		}
		const StaticArray<VkExtensionProperties> ext = getDeviceExtension(d);
		if (ext.size() == 0u) {
			continue;
		}
		DeviceProperty dev_props = getPhysicalDeviceProperty(d);
		
		const StaticArray<VkSurfaceFormatKHR> surface_format = getSurfaceFormat(d, surface);
		if (surface_format.size() == 0u) {
			continue;
		}
		const StaticArray<VkPresentModeKHR> surface_present = getSurfacePresentMode(d, surface);
		if (surface_present.size() == 0u) {
			continue;
		}

		////////////////////////////
		/// Verifying stuff
		///////////////////////////
		if (!isPhysicalDeviceSuitable(std::get<0u>(*dev_props).properties, ext.toSpan(),
			requirement.DeviceType, requirement.DeviceExtension)) {
			continue;
		}
		if (!isSurfaceFormatSuitable(surface_format.toSpan(), requirement.Format, requirement.ColourSpace)) {
			continue;
		}
		if (!isPresentModeSuitable(surface_present.toSpan(), requirement.PresentMode)) {
			continue;
		}

		const optional<uint32_t> rendering_queue_opt = findRenderingQueueFamily(qf.toSpan(), requirement.QueueFamilyCapability);
		if (!rendering_queue_opt) {
			continue;
		}
		const optional<uint32_t> presenting_queue_opt = findPresentingQueueFamily(d, surface, static_cast<uint32_t>(qf.size()));
		if (!presenting_queue_opt) {
			continue;
		}

		return VulkanContext {
			.TotalPhysicalDevice = static_cast<uint32_t>(device.size()),
			.TotalQueueFamily = static_cast<uint32_t>(qf.size()),

			.DeviceProperty = move(dev_props),

			.PhysicalDevice = d,
			.RenderingQueueFamily = *rendering_queue_opt,
			.PresentingQueueFamily = *presenting_queue_opt
		};
	}

	throw runtime_error("No suitable physical device was found that meets all requirements.");
}

ContextManager::SwapchainImage ContextManager::querySwapchainImage(const VkDevice device, const VkSwapchainKHR sc, const VkFormat format) {
	uint32_t img_count;
	CHECK_VULKAN_ERROR(vkGetSwapchainImagesKHR(device, sc, &img_count, nullptr));

	StaticArray<VkImage> sc_raw_img(img_count);
	CHECK_VULKAN_ERROR(vkGetSwapchainImagesKHR(device, sc, &img_count, sc_raw_img.data()));

	/********************************
	 * Create swap chain image view
	 *******************************/
	SwapchainImage sc_img(img_count);
	std::ranges::transform(sc_raw_img.toSpan(), sc_img.data(), [device, format](const VkImage img) {
		return std::make_pair(
			img,
			ImageManager::createFullImageView({
				.Device = device,
				.Image = img,
				.ViewType = VK_IMAGE_VIEW_TYPE_2D,
				.Format = format,
				.Aspect = VK_IMAGE_ASPECT_COLOR_BIT
			})
		);
	});
	return sc_img;
}