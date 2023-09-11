#include "MasterEngine.hpp"

#include "ContextManager.hpp"
#include "Abstraction/CommandBufferManager.hpp"
#include "Abstraction/ImageManager.hpp"
#include "Abstraction/SemaphoreManager.hpp"
#include "Abstraction/PipelineBarrier.hpp"
#include "../Common/ErrorHandler.hpp"

#include <Volk/volk.h>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include <iterator>
#include <algorithm>
#include <limits>
#include <utility>

#include <vector>
#include <tuple>
#include <span>

#include <stdexcept>
#include <cassert>

using glm::uvec2;
using glm::clamp;

using std::ostream, std::endl;
using std::ostream_iterator;
using std::move;

using std::array, std::span, std::pair, std::tuple, std::vector;
using std::make_pair, std::make_tuple;

using std::ranges::transform, std::ranges::generate, std::ranges::copy;

using std::runtime_error;
using std::numeric_limits;

using namespace LearnVulkan;
namespace VKO = VulkanObject;
namespace CTX = ContextManager;

struct MasterEngine::DebugCallbackUserData {

	ostream* MessageStream;
	const VulkanContext* Context;

};

namespace {

	constexpr array RequiredLayer = { "VK_LAYER_KHRONOS_validation" };
	constexpr array RequiredExtension = {
		VK_KHR_16BIT_STORAGE_EXTENSION_NAME,
		VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
		VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
		VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME,
		VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
		VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME,
		VK_KHR_MAINTENANCE_1_EXTENSION_NAME,
		VK_KHR_MAINTENANCE_4_EXTENSION_NAME,
		VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
		VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
		VK_KHR_RAY_QUERY_EXTENSION_NAME,
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_SWAPCHAIN_MUTABLE_FORMAT_EXTENSION_NAME,
		VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,

		VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME,
		VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME,
		VK_EXT_INDEX_TYPE_UINT8_EXTENSION_NAME
	};

	constexpr CTX::DeviceRequirement ContextRequirement = {
		.DeviceType = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU,
		.DeviceExtension = RequiredExtension,
		.QueueFamilyCapability = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT,

		.Format = VK_FORMAT_B8G8R8A8_SRGB,
		.ColourSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
		.PresentMode = VK_PRESENT_MODE_FIFO_KHR
	};
	constexpr VkFormat SwapChainImageViewFormat = VK_FORMAT_R8G8B8A8_UNORM;
	constexpr auto SwapChainCompatibleImageFormat = array {
		::ContextRequirement.Format,
		::SwapChainImageViewFormat
	};

	struct EngineSwapchainCreateInfo {

		VkFormat ImageFormat;
		VkColorSpaceKHR ImageColourSpace;
		VkPresentModeKHR Presentation;

	};

	VKO::Instance createInstance(ostream& msg) {
		constexpr static uint32_t version = VK_MAKE_API_VERSION(0u, 0u, 16u, 7u);//v0.16.7
		constexpr static VkApplicationInfo app_info {
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pApplicationName = "Vulkan Tutorial",
			.applicationVersion = version,
			.pEngineName = "Learn Vulkan Demo Engine",
			.engineVersion = version,
			.apiVersion = VK_API_VERSION_1_3
		};

		/******************************
		 * Layer extension verification
		 ******************************/
		const auto [all_layer] = CTX::verifyLayerSupport(RequiredLayer);
		msg << "All instance layer supported by the application:\n";
		transform(all_layer.toSpan(), ostream_iterator<const char*>(msg, "\n"),
			[](const VkLayerProperties& l) constexpr noexcept { return l.layerName; });
		msg << "---------------------------------------------" << endl;

		/*************************
		 * Extension requirement
		 ************************/
		uint32_t req_layer_count = 0u;
		//this pointer should never be null because we have checked that Vulkan is available on this system on initialisation
		const char* const* const req_layer = glfwGetRequiredInstanceExtensions(&req_layer_count);
		assert(req_layer);
		msg << "Instance layer required by the application:\n";
		copy(span(req_layer, req_layer_count), ostream_iterator<const char*>(msg, "\n"));
		msg << "------------------------------" << endl;

		//add additional instance extension under debug build
		auto enabled_layer = vector(req_layer, req_layer + req_layer_count);
		if constexpr (EngineSetting::EnableValidation) {
			//no need to check the existence of this extension, because it is implicitly enabled by validation layer
			enabled_layer.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		/***********************
		 * Instance Creation
		 ***********************/
		VkInstanceCreateInfo ins_info {
			.sType = VkStructureType::VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pApplicationInfo = &app_info,
			.enabledExtensionCount = static_cast<uint32_t>(enabled_layer.size()),
			.ppEnabledExtensionNames = enabled_layer.data()
		};
		if constexpr (EngineSetting::EnableValidation) {
			ins_info.enabledLayerCount = static_cast<uint32_t>(RequiredLayer.size());
			ins_info.ppEnabledLayerNames = RequiredLayer.data();
		}
		return VKO::createInstance(ins_info);
	}

	inline VKO::SurfaceKHR createSurface(GLFWwindow* const canvas, const VkInstance instance) {
		VkSurfaceKHR surface;
		CHECK_VULKAN_ERROR(glfwCreateWindowSurface(instance, canvas, nullptr, &surface));
		return VKO::createSurfaceKHR(instance, surface);
	}

	template<class TCallbackData>
	VKO::DebugUtilsMessengerEXT setupDebugCallback(const VulkanContext& ctx, const TCallbackData* const callback_data) {
		constexpr static auto handleMessage = [](const VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
			VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT* const pCallbackData,
			void* const data) -> VkBool32 {
			const TCallbackData& callback_data = *reinterpret_cast<const TCallbackData*>(const_cast<const void*>(data));
			
			if (callback_data.Context->isMessageDisabled(pCallbackData->messageIdNumber)) {
				return VK_FALSE;
			}
			
			*callback_data.MessageStream << "Validation Layer: " << pCallbackData->pMessage << endl;
			if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
				throw runtime_error("Vulkan has encountered a fatal error!");
			}

			return VK_FALSE;
		};
		const VkDebugUtilsMessengerCreateInfoEXT dbg_msg_info {
			.sType = VkStructureType::VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
			.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
			.pfnUserCallback = handleMessage,
			.pUserData = const_cast<TCallbackData*>(callback_data)
		};
		return VKO::createDebugUtilsMessengerEXT(ctx.Instance, dbg_msg_info);
	}

	tuple<VKO::Device, VkQueue, VkQueue> createLogicalDevice(const CTX::VulkanContext& ctx) {
		const uint32_t render_queue_idx = ctx.RenderingQueueFamily,
			present_queue_idx = ctx.PresentingQueueFamily;
		/*********************************
		 * Create logical device
		 *********************************/
		constexpr static float priority = 0.25f;
		array<VkDeviceQueueCreateInfo, 2u> queue_info;
		//render queue
		queue_info[0] = {
			.sType = VkStructureType::VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = render_queue_idx,
			.queueCount = 1u,
			.pQueuePriorities = &priority
		};
		//present queue, which reuses most parameters from the render queue
		queue_info[1] = queue_info[0];
		queue_info[1].queueFamilyIndex = present_queue_idx;

		//Vulkan specification requires all queue family indices to be distinct,
		//so if they are the same then just creating one queue is sufficient.
		const uint32_t queue_info_count = render_queue_idx == present_queue_idx ? 1u : static_cast<uint32_t>(queue_info.size());

		//TODO: we omit most device features for now, add them later if needed
		VkPhysicalDeviceRayQueryFeaturesKHR ray_query {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
			.rayQuery = VK_TRUE
		};
		VkPhysicalDeviceAccelerationStructureFeaturesKHR accel_struct {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
			.pNext = &ray_query,
			.accelerationStructure = VK_TRUE
		};
		VkPhysicalDeviceMaintenance4Features maintenance_4 {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES,
			.pNext = &accel_struct,
			.maintenance4 = VK_TRUE
		};
		VkPhysicalDevice16BitStorageFeatures storage_16bit {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES,
			.pNext = &maintenance_4,
			.storageBuffer16BitAccess = VK_TRUE
		};
		VkPhysicalDeviceDynamicRenderingFeatures dynamic_rendering {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
			.pNext = &storage_16bit,
			.dynamicRendering = VK_TRUE
		};
		VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT graphics_lib {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT,
			.pNext = &dynamic_rendering,
			.graphicsPipelineLibrary = VK_TRUE
		};
		VkPhysicalDeviceDescriptorBufferFeaturesEXT des_buf {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT,
			.pNext = &graphics_lib,
			.descriptorBuffer = VK_TRUE,
			.descriptorBufferPushDescriptors = VK_TRUE
		};
		VkPhysicalDeviceBufferDeviceAddressFeatures dev_address {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
			.pNext = &des_buf,
			.bufferDeviceAddress = VK_TRUE
		};
		VkPhysicalDeviceTimelineSemaphoreFeatures timeline_sema {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES,
			.pNext = &dev_address,
			.timelineSemaphore = VK_TRUE
		};
		VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures sep_depth_stencil {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES,
			.pNext = &timeline_sema,
			.separateDepthStencilLayouts = VK_TRUE
		};
		VkPhysicalDeviceSynchronization2Features sync2 {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
			.pNext = &sep_depth_stencil,
			.synchronization2 = VK_TRUE
		};
		VkPhysicalDeviceIndexTypeUint8FeaturesEXT uint8_index {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INDEX_TYPE_UINT8_FEATURES_EXT,
			.pNext = &sync2,
			.indexTypeUint8 = VK_TRUE
		};
		constexpr static VkPhysicalDeviceFeatures feature10 {
			.tessellationShader = VK_TRUE,
			.sampleRateShading = VK_TRUE,
			.samplerAnisotropy = VK_TRUE,
			.shaderFloat64 = VK_TRUE,
			.shaderInt64 = VK_TRUE,
			.shaderInt16 = VK_TRUE
		};
		const VkDeviceCreateInfo dev_info {
			.sType = VkStructureType::VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.pNext = &uint8_index,
			.queueCreateInfoCount = queue_info_count,
			.pQueueCreateInfos = queue_info.data(),
			.enabledExtensionCount = static_cast<uint32_t>(RequiredExtension.size()),
			.ppEnabledExtensionNames = RequiredExtension.data(),
			.pEnabledFeatures = &feature10
		};
		VKO::Device device = VKO::createDevice(ctx.PhysicalDevice, dev_info);

		/**********************************
		 * Retrieve queue
		 **********************************/
		VkQueue render_queue, present_queue;
		vkGetDeviceQueue(device, render_queue_idx, 0u, &render_queue);
		vkGetDeviceQueue(device, present_queue_idx, 0u, &present_queue);

		return make_tuple(move(device), render_queue, present_queue);
	}

	inline VKO::Allocator createGlobalVma(const VkInstance instance, const VkPhysicalDevice gpu, const VkDevice device) {
		const VmaVulkanFunctions alloc_func {
			.vkGetInstanceProcAddr = vkGetInstanceProcAddr,
			.vkGetDeviceProcAddr = vkGetDeviceProcAddr
		};
		const VmaAllocatorCreateInfo alloc_info {
			//HACK: we are not doing multithreading in this tutorial, but remember to remove it if we need to do so.
			.flags = VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT | VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
			.physicalDevice = gpu,
			.device = device,
			.pVulkanFunctions = &alloc_func,
			.instance = instance,
			.vulkanApiVersion = VK_API_VERSION_1_3
		};
		return VKO::createAllocator(alloc_info);
	}

	pair<VKO::SwapchainKHR, VkExtent2D> createSwapchain(GLFWwindow* const canvas, const VulkanContext& ctx,
		const VkSurfaceKHR surface, const EngineSwapchainCreateInfo& swap_chain_info, const VkSwapchainKHR old_swap_chain = VK_NULL_HANDLE) {
		const auto chooseExtent = [](const uvec2 current, const uvec2 actual, const uvec2 min, const uvec2 max) noexcept -> uvec2 {
			//let the application to choose the optimal self itself
			if (current.x != numeric_limits<uint32_t>::max()) {
				return current;
			}
			return clamp(actual, min, max);
		};
		const auto [format, colour_space, present_mode] = swap_chain_info;
		const auto [render_queue, present_queue] = ctx.QueueIndex;

		VkSurfaceCapabilitiesKHR sur_cap;
		CHECK_VULKAN_ERROR(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx.PhysicalDevice, surface, &sur_cap));

		/********************************
		 * Determine valid parameter
		 ********************************/
		int w, h;
		glfwGetFramebufferSize(canvas, &w, &h);
		const VkExtent2D& current = sur_cap.currentExtent,
			&min = sur_cap.minImageExtent,
			&max = sur_cap.maxImageExtent;
		const uvec2 chosen_vec = chooseExtent(
			uvec2(current.width, current.height),
			uvec2(w, h),
			uvec2(min.width, min.height),
			uvec2(max.width, max.height)
		);
		const VkExtent2D chosen_extent = { chosen_vec.x, chosen_vec.y };

		const uint32_t min_count = sur_cap.minImageCount,
			max_count = sur_cap.maxImageCount,
			//add one to the min count to give us extra room for synchronisation, so we don't need to wait for the driver
			//if max count is zero, there is no upper limit
			image_count = std::min(min_count + 1u, max_count == 0u ? numeric_limits<uint32_t>::max() : max_count);

		/********************************
		 * Create swap chain
		 *******************************/
		constexpr static VkImageFormatListCreateInfo swapchain_format {
			.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO,
			.viewFormatCount = static_cast<uint32_t>(::SwapChainCompatibleImageFormat.size()),
			.pViewFormats = ::SwapChainCompatibleImageFormat.data()
		};
		VkSwapchainCreateInfoKHR swapchain_info {
			.sType = VkStructureType::VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.pNext = &swapchain_format,
			.flags = VK_SWAPCHAIN_CREATE_MUTABLE_FORMAT_BIT_KHR,
			.surface = surface,
			.minImageCount = image_count,
			.imageFormat = format,
			.imageColorSpace = colour_space,
			.imageExtent = chosen_extent,
			.imageArrayLayers = 1u,
			.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.preTransform = sur_cap.currentTransform,
			.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			.presentMode = present_mode,
			.clipped = VK_TRUE,
			.oldSwapchain = old_swap_chain
		};
		//TODO: look into swap chain ownership transfer later
		//share the swap chain if queue families are distinct
		if (render_queue != present_queue) {
			const auto sharing_queue = array { render_queue, present_queue };
			swapchain_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			swapchain_info.queueFamilyIndexCount = static_cast<uint32_t>(sharing_queue.size());
			swapchain_info.pQueueFamilyIndices = sharing_queue.data();
		} else {
			swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		}

		return make_pair(VKO::createSwapchainKHR(ctx.Device, swapchain_info), chosen_extent);
	}

	//Return three of such, two semaphores for rendering and presenting queue, and one for host-device barrier.
	inline array<VKO::Semaphore, 3u> createSyncPrimitive(const VulkanContext& ctx) {
		return {
			SemaphoreManager::createBinarySemaphore(ctx.Device),
			SemaphoreManager::createBinarySemaphore(ctx.Device),
			SemaphoreManager::createTimelineSemaphore(ctx.Device, 0ull)
		};
	}

}

MasterEngine::MasterEngine(GLFWwindow* const canvas, const Camera::CameraData& camera_data, ostream& msg) :
	DbgCbUserData(EngineSetting::EnableValidation ? std::make_unique<DebugCallbackUserData>() : nullptr),
	AttachedRenderer(nullptr), FrameInFlightIndex(0u) {
	/*********************************
	 * Application context creation
	 ********************************/
	{
		//HACK: It's actually not a very good practice to pass the entire context structure into each create functions
		//while not all members are fully initialised, and relying on the fact that those functions only use subset of initialised members.
		//It is done just because of my laziness to reduce typing many function arguments.
		VKO::Instance instance = createInstance(msg);
		volkLoadInstance(instance);
		//need to create a surface first because we need to find surface format when selecting physical device
		this->Surface = createSurface(canvas, instance);

		//select an appropriate physical device
		const CTX::VulkanContext context = CTX::selectPhysicalDevice(instance, this->Surface, ContextRequirement);
	
		auto [logical_device, render_queue, present_queue] = createLogicalDevice(context);
		volkLoadDevice(logical_device);

		this->Context.Instance = move(instance);
		this->Context.PhysicalDevice = context.PhysicalDevice;
		this->Context.Device = move(logical_device);

		this->Context.Allocator = createGlobalVma(this->Context.Instance, this->Context.PhysicalDevice, this->Context.Device);
		this->Context.CommandPool = {
			.Reshape = CommandBufferManager::createCommandPool(this->Context.Device, { }, context.RenderingQueueFamily),
			.Transient = CommandBufferManager::createCommandPool(this->Context.Device,
				VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, context.RenderingQueueFamily),
			.General = CommandBufferManager::createCommandPool(this->Context.Device,
				VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, context.RenderingQueueFamily)
		};
		generate(this->Context.CommandPool.InFlightCommandPool, [device = *this->Context.Device, qf = context.RenderingQueueFamily]()
			{ return CommandBufferManager::createCommandPool(device, { }, qf); });

		this->Context.Queue = {
			.Render = render_queue,
			.Present = present_queue
		};
		this->Context.QueueIndex = {
			.Render = context.RenderingQueueFamily,
			.Present = context.PresentingQueueFamily
		};

		//////////////////////
		/// Logging
		/////////////////////
		msg << "Found " << context.TotalPhysicalDevice << " physical device\n";
		const auto& [dev10_struct, dev11, dev12, dev13, descriptor_buf] = *context.DeviceProperty;
		const VkPhysicalDeviceProperties& dev10 = dev10_struct.properties;

		msg << "Select physical device:\n";
		msg << "Device name: " << dev10.deviceName << '\n';
		msg << "Device ID: " << dev10.deviceID << '\n';
		msg << "API version: " << dev10.apiVersion << '\n';
		msg << "Driver version: " << dev10.driverVersion << '\n';
		msg << "Vendor ID: " << dev10.vendorID << '\n';
		msg << "Driver name: " << dev12.driverName << '\n';
		msg << "Driver info: " << dev12.driverInfo << '\n';
		msg << "-------------------------------------------------" << endl;

		msg << "Found " << context.TotalQueueFamily << " device queue family\n";
		msg << "Select rendering queue family " << context.RenderingQueueFamily << '\n';
		msg << "Select presenting queue family " << context.PresentingQueueFamily << '\n';
		msg << "---------------------------------------------------------------------------" << endl;

		this->Context.PhysicalDeviceProperty = {
			.DescriptorBuffer = descriptor_buf
		};
	}

	/*****************
	 * Presentation
	 *****************/
	if constexpr (EngineSetting::EnableValidation) {
		*this->DbgCbUserData = {
			.MessageStream = &msg,
			.Context = &this->Context
		};

		this->DebugMessage = setupDebugCallback(this->Context, this->DbgCbUserData.get());
	}

	this->createPresentation(canvas);
	msg << this->SwapChainImage.size() << " swap chain image has been queried" << endl;

	/*******************
	 * Renderer
	 ******************/
	generate(this->DrawSync, [&ctx = std::as_const(this->Context)]() {
		auto [available_sema, finish_sema, wait_frame] = createSyncPrimitive(ctx);
		return DrawSynchronisationPrimitive {
			.ImageAvailable = move(available_sema),
			.RenderFinish = move(finish_sema),
			.WaitFrame = move(wait_frame),
			.FrameCounter = 0ull
		};
	});

	this->SceneCamera.emplace(Camera::CreateInfo {
		.Context = &this->Context,
		.CameraInfo = &camera_data
	});
}

MasterEngine::~MasterEngine() = default;

inline void MasterEngine::createPresentation(GLFWwindow* const canvas) {
	const EngineSwapchainCreateInfo swapchain_info {
		.ImageFormat = ContextRequirement.Format,
		.ImageColourSpace = ContextRequirement.ColourSpace,
		.Presentation = ContextRequirement.PresentMode
	};
	auto [swap_chain, swap_chain_extent] = createSwapchain(canvas, this->Context, this->Surface, swapchain_info);

	this->SwapChain = move(swap_chain);
	this->SwapChainExtent = swap_chain_extent;
	this->SwapChainImage = CTX::querySwapchainImage(this->Context.Device, this->SwapChain, ::SwapChainImageViewFormat);
}

const VulkanContext& MasterEngine::context() const noexcept {
	return this->Context;
}

Camera& MasterEngine::camera() noexcept {
	return *this->SceneCamera;
}

void MasterEngine::attachRenderer(RendererInterface* const renderer) {
	this->AttachedRenderer = renderer;
	if (this->AttachedRenderer) {
		//reshape to allocate initial rendering memory
		this->AttachedRenderer->reshape({
			.Context = &this->Context,
			.Extent = this->SwapChainExtent
		});
	}
}

void MasterEngine::reshape(GLFWwindow* const canvas) {
	{
		//handle minimisation
		int w = 0, h = 0;
		glfwGetFramebufferSize(canvas, &w, &h);
		while (w == 0 || h == 0) {
			glfwGetFramebufferSize(canvas, &w, &h);
			glfwWaitEvents();
		}
		CHECK_VULKAN_ERROR(vkDeviceWaitIdle(this->Context.Device));

		//delete old presentation context first in the correct order
		this->SwapChain->reset();
		CHECK_VULKAN_ERROR(vkResetCommandPool(this->Context.Device, this->Context.CommandPool.Reshape, { }));

		//Then recreate all of them, swap chain extent will be updated when presentation is re-created.
		this->createPresentation(canvas);
		this->attachRenderer(this->AttachedRenderer);
	}
	//update camera
	{
		const auto [w, h] = this->SwapChainExtent;
		this->SceneCamera->setAspect(w, h);
	}
}

void MasterEngine::draw(const double delta_time) const {
	const auto& [image_available_sema, render_finish_sema, wait_frame, frame_counter] = this->DrawSync[this->FrameInFlightIndex];
	/****************************
	 * Rendering
	 ***************************/
	//wait for previous rendering on the same in-flight index to finish before starting the current
	SemaphoreManager::wait<1u>(this->Context.Device, { }, {{{ wait_frame, frame_counter++ }}});
	//it is cheaper to reset the command pool globally than issuing reset to individual command buffer
	CHECK_VULKAN_ERROR(vkResetCommandPool(this->Context.Device,
		this->Context.CommandPool.InFlightCommandPool[this->FrameInFlightIndex], { }));

	this->SceneCamera->update(this->FrameInFlightIndex);

	//acquire image from swap chain
	uint32_t image_index;
	CHECK_VULKAN_ERROR(vkAcquireNextImageKHR(this->Context.Device, this->SwapChain, numeric_limits<uint64_t>::max(),
		image_available_sema, VK_NULL_HANDLE, &image_index));

	//compose draw command for next frame onto the requested image
	const auto& [present_img, present_img_view] = this->SwapChainImage[image_index];
	const LearnVulkan::RendererInterface::DrawInfo draw_info {
		.Context = &this->Context,
		.Camera = &*this->SceneCamera,

		.DeltaTime = delta_time,
		.FrameInFlightIndex = this->FrameInFlightIndex,

		.Viewport = {
			.x = 0.0f,
			.y = 1.0f * this->SwapChainExtent.height,//lower-left origin
			.width = 1.0f * this->SwapChainExtent.width,
			.height = -1.0f * this->SwapChainExtent.height,//Y is inverted
			.minDepth = 0.0f,
			.maxDepth = 1.0f
		},
		.DrawArea = {
			.offset = { 0, 0 },
			.extent = this->SwapChainExtent
		},

		.PresentImage = present_img,
		.PresentImageView = present_img_view
	};
	const auto [draw_cmd, wait_stage] = this->AttachedRenderer->draw(draw_info);

	/*************************
	 * Signal for presentation
	 *************************/
	{
		const VkSemaphore wait_sema = image_available_sema,
			signal_sema = render_finish_sema;
		const VkSwapchainKHR swap_chain = this->SwapChain;

		const VkPresentInfoKHR present_info {
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = 1u,
			.pWaitSemaphores = &signal_sema,
			.swapchainCount = 1u,
			.pSwapchains = &swap_chain,
			.pImageIndices = &image_index
		};
		//submit draw command
		CommandBufferManager::submit<1u, 1u, 2u>({ this->Context.Device, this->Context.Queue.Render },
			{ draw_cmd },
			{{{ wait_sema, wait_stage }}},
			{{
				//looks like WSI only supports binary semaphore
				{ signal_sema, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT },
				{ wait_frame, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, frame_counter }
			}}, VK_NULL_HANDLE);
		//return swap chain image back
		CHECK_VULKAN_ERROR(vkQueuePresentKHR(this->Context.Queue.Present, &present_info));
	}

	this->FrameInFlightIndex = (this->FrameInFlightIndex + 1u) % EngineSetting::MaxFrameInFlight;
}