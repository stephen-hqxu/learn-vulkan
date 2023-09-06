#pragma once

#include <Volk/volk.h>
#include <vma/vk_mem_alloc.h>

#include <memory>
#include <type_traits>
#include <utility>

#define VULKAN_OBJECT_DELETER_COMMON_MEMBER(POINTER) using pointer = POINTER; \
void operator()(pointer) const noexcept
#define DECLARE_VULKAN_OBJECT_DELETER(DEL_NAME, POINTER) struct DEL_NAME { \
	VULKAN_OBJECT_DELETER_COMMON_MEMBER(POINTER); \
}
#define CREATE_VULKAN_OBJECT_ALIAS(ALIAS_NAME, OBJECT, DEL) \
using ALIAS_NAME = _Internal::UniqueHandle<OBJECT, _Internal::DEL>

namespace LearnVulkan {

	/**
	 * @brief Vulkan object wrapped over unique_ptr with automatic lifetime management.
	 * All creation functions are analogous to the standard Vulkan functions.
	*/
	namespace VulkanObject {

		//definition of each deleter should be placed here
		namespace _Internal {

			/**************************
			 * Vulkan Memory Allocator
			 *************************/

			DECLARE_VULKAN_OBJECT_DELETER(AllocatorDestroyer, VmaAllocator);

			struct AllocationFreer {

				VULKAN_OBJECT_DELETER_COMMON_MEMBER(VmaAllocation);

				VmaAllocator Allocator;

			};

			/************************
			 * Vulkan API
			 ************************/

			DECLARE_VULKAN_OBJECT_DELETER(InstanceDestroyer, VkInstance);
			DECLARE_VULKAN_OBJECT_DELETER(DeviceDestroyer, VkDevice);

			struct DebugUtilsMessengerEXTDestroyer {

				VULKAN_OBJECT_DELETER_COMMON_MEMBER(VkDebugUtilsMessengerEXT);

				VkInstance Instance;

			};

			struct SurfaceKHRDestroyer {

				VULKAN_OBJECT_DELETER_COMMON_MEMBER(VkSurfaceKHR);

				VkInstance Instance;

			};

			struct SwapchainKHRDestroyer {

				VULKAN_OBJECT_DELETER_COMMON_MEMBER(VkSwapchainKHR);

				VkDevice Device;

			};

			struct ImageDestroyer {

				VULKAN_OBJECT_DELETER_COMMON_MEMBER(VkImage);

				VkDevice Device;

			};

			struct ImageViewDestroyer {

				VULKAN_OBJECT_DELETER_COMMON_MEMBER(VkImageView);

				VkDevice Device;

			};

			struct ShaderModuleDestroyer {

				VULKAN_OBJECT_DELETER_COMMON_MEMBER(VkShaderModule);

				VkDevice Device;

			};

			struct PipelineDestroyer {

				VULKAN_OBJECT_DELETER_COMMON_MEMBER(VkPipeline);

				VkDevice Device;

			};

			struct PipelineLayoutDestroyer {

				VULKAN_OBJECT_DELETER_COMMON_MEMBER(VkPipelineLayout);

				VkDevice Device;

			};

			struct RenderPassDestroyer {

				VULKAN_OBJECT_DELETER_COMMON_MEMBER(VkRenderPass);

				VkDevice Device;

			};

			struct FramebufferDestroyer {

				VULKAN_OBJECT_DELETER_COMMON_MEMBER(VkFramebuffer);

				VkDevice Device;

			};

			struct CommandPoolDestroyer {

				VULKAN_OBJECT_DELETER_COMMON_MEMBER(VkCommandPool);

				VkDevice Device;

			};

			struct CommandBuffersFreer {

				VULKAN_OBJECT_DELETER_COMMON_MEMBER(VkCommandBuffer*);

				uint32_t Count;
				VkDevice Device;
				VkCommandPool CmdPool;

			};

			struct SemaphoreDestroyer {

				VULKAN_OBJECT_DELETER_COMMON_MEMBER(VkSemaphore);

				VkDevice Device;

			};

			struct BufferDestroyer {
				
				VULKAN_OBJECT_DELETER_COMMON_MEMBER(VkBuffer);

				VkDevice Device;

			};

			struct DescriptorSetLayoutDestroyer {

				VULKAN_OBJECT_DELETER_COMMON_MEMBER(VkDescriptorSetLayout);

				VkDevice Device;

			};

			struct DescriptorPoolDestroyer {

				VULKAN_OBJECT_DELETER_COMMON_MEMBER(VkDescriptorPool);

				VkDevice Device;

			};

			struct SamplerDestroyer {

				VULKAN_OBJECT_DELETER_COMMON_MEMBER(VkSampler);

				VkDevice Device;

			};

			struct MemoryUnmapper {

				void operator()(void*) const noexcept;

				VmaAllocator Allocator;
				VmaAllocation Allocation;

			};

			struct AccelerationStructureKHRDestroyer {

				VULKAN_OBJECT_DELETER_COMMON_MEMBER(VkAccelerationStructureKHR);

				VkDevice Device;

			};

			struct QueryPoolDestroyer {

				VULKAN_OBJECT_DELETER_COMMON_MEMBER(VkQueryPool);

				VkDevice Device;

			};

			/**
			 * @brief A simple wrapper over unique_ptr type.
			 * @tparam THandle The type of the handle to be wrapped over.
			 * @tparam TDeleter The type of deleter.
			*/
			template<class THandle, class TDeleter>
			requires requires(THandle handle, TDeleter del) {
				std::is_pointer_v<THandle> && std::is_trivial_v<TDeleter>;
				del(handle);
			}
			class UniqueHandle {
			public:

				using ManagedHandle = std::unique_ptr<THandle, TDeleter>;/**< Generic type for different smart handle type. */
				ManagedHandle Handle;/**< The managed handle object. */

				/**
				 * @brief Default construction with no managed handle.
				*/
				constexpr UniqueHandle() noexcept = default;

				/**
				 * @brief Construct a managed handle from an existing handle.
				 * @param handle The exiting handle to be managed.
				 * @param deleter Provide a deleter instance, or it will be default constructed.
				*/
				UniqueHandle(const THandle handle, TDeleter&& deleter = TDeleter { }) noexcept : Handle(handle, std::forward<TDeleter>(deleter)) {

				}

				UniqueHandle(const UniqueHandle&) = delete;

				UniqueHandle(UniqueHandle&&) noexcept = default;

				UniqueHandle& operator=(const UniqueHandle&) = delete;

				UniqueHandle& operator=(UniqueHandle&&) noexcept = default;

				~UniqueHandle() = default;

				constexpr operator THandle() const noexcept {
					return this->Handle.get();
				}

				constexpr THandle operator*() const noexcept {
					return this->Handle.get();
				}

				constexpr ManagedHandle* operator->() noexcept {
					return &this->Handle;
				}

				constexpr const ManagedHandle* operator->() const noexcept {
					return &this->Handle;
				}

			};

			void* mapAllocation(VmaAllocator, VmaAllocation);

		}

		CREATE_VULKAN_OBJECT_ALIAS(Allocator, VmaAllocator, AllocatorDestroyer);/**< VmaAllocator */
		CREATE_VULKAN_OBJECT_ALIAS(Allocation, VmaAllocation, AllocationFreer);/**< VmaAllocation */

		CREATE_VULKAN_OBJECT_ALIAS(Instance, VkInstance, InstanceDestroyer);/**< VkInstance */
		CREATE_VULKAN_OBJECT_ALIAS(Device, VkDevice, DeviceDestroyer);/**< VkDevice */
		CREATE_VULKAN_OBJECT_ALIAS(Image, VkImage, ImageDestroyer);/**< VkImage */
		CREATE_VULKAN_OBJECT_ALIAS(ImageView, VkImageView, ImageViewDestroyer);/**< VkImageView */
		CREATE_VULKAN_OBJECT_ALIAS(ShaderModule, VkShaderModule, ShaderModuleDestroyer);/**< VkShaderModule */
		CREATE_VULKAN_OBJECT_ALIAS(Pipeline, VkPipeline, PipelineDestroyer);/**< VkPipeline */
		CREATE_VULKAN_OBJECT_ALIAS(PipelineLayout, VkPipelineLayout, PipelineLayoutDestroyer);/**< VkPipelineLayout */
		CREATE_VULKAN_OBJECT_ALIAS(RenderPass, VkRenderPass, RenderPassDestroyer);/**< VkRenderPass */
		CREATE_VULKAN_OBJECT_ALIAS(Framebuffer, VkFramebuffer, FramebufferDestroyer);/**< VkFramebuffer */
		CREATE_VULKAN_OBJECT_ALIAS(CommandPool, VkCommandPool, CommandPoolDestroyer);/**< VkCommandPool */
		CREATE_VULKAN_OBJECT_ALIAS(Semaphore, VkSemaphore, SemaphoreDestroyer);/**< VkSemaphore */
		CREATE_VULKAN_OBJECT_ALIAS(Buffer, VkBuffer, BufferDestroyer);/**< VkBuffer */
		CREATE_VULKAN_OBJECT_ALIAS(DescriptorSetLayout, VkDescriptorSetLayout, DescriptorSetLayoutDestroyer);/**< VkDescriptorSetLayout */
		CREATE_VULKAN_OBJECT_ALIAS(DescriptorPool, VkDescriptorPool, DescriptorPoolDestroyer);/**< VkDescriptorPool */
		CREATE_VULKAN_OBJECT_ALIAS(Sampler, VkSampler, SamplerDestroyer);/**< VkSampler */
		CREATE_VULKAN_OBJECT_ALIAS(QueryPool, VkQueryPool, QueryPoolDestroyer);/**< VkQueryPool */

		CREATE_VULKAN_OBJECT_ALIAS(SurfaceKHR, VkSurfaceKHR, SurfaceKHRDestroyer);/**< VkSurfaceKHR */
		CREATE_VULKAN_OBJECT_ALIAS(DebugUtilsMessengerEXT, VkDebugUtilsMessengerEXT, DebugUtilsMessengerEXTDestroyer);/**< VkDebugUtilsMessengerEXT */
		CREATE_VULKAN_OBJECT_ALIAS(SwapchainKHR, VkSwapchainKHR, SwapchainKHRDestroyer);/**< VkSwapchainKHR */
		CREATE_VULKAN_OBJECT_ALIAS(AccelerationStructureKHR, VkAccelerationStructureKHR, AccelerationStructureKHRDestroyer);/**< VkAccelerationStructureKHR */

		//The order of memory and buffer is important!
		//When the buffer is created, it is empty, then we allocate memory and bind it to the buffer.
		//So we should destroy the buffer (the container) first.
		using BufferAllocation = std::pair<Allocation, Buffer>;/**< VmaAllocation, VkBuffer */
		using ImageAllocation = std::pair<Allocation, Image>;/**< VmaAllocation, VkImage */
		template<class T>
		using MappedAllocation = std::unique_ptr<T, _Internal::MemoryUnmapper>;/**< Pointer to mapped allocation */

		//Objects that support batch allocation and free, it's faster to allocate and destroy all of them in one API call.
		using CommandBufferArray = std::unique_ptr<VkCommandBuffer[], _Internal::CommandBuffersFreer>;

		Allocator createAllocator(const VmaAllocatorCreateInfo&);
		BufferAllocation createBufferFromAllocator(VkDevice, VmaAllocator, const VkBufferCreateInfo&, const VmaAllocationCreateInfo&);
		ImageAllocation createImageFromAllocator(VkDevice, VmaAllocator, const VkImageCreateInfo&, const VmaAllocationCreateInfo&);

		template<class T>
		inline MappedAllocation<T> mapAllocation(const VmaAllocator allocator, const VmaAllocation allocation) {
			void* const mapped = _Internal::mapAllocation(allocator, allocation);
			return MappedAllocation<T>(new(mapped) T, { allocator, allocation });
		}

		Instance createInstance(const VkInstanceCreateInfo&);
		Device createDevice(VkPhysicalDevice, const VkDeviceCreateInfo&);
		ImageView createImageView(VkDevice, const VkImageViewCreateInfo&);
		ShaderModule createShaderModule(VkDevice, const VkShaderModuleCreateInfo&);
		Pipeline createGraphicsPipeline(VkDevice, VkPipelineCache, const VkGraphicsPipelineCreateInfo&);
		Pipeline createComputePipeline(VkDevice, VkPipelineCache, const VkComputePipelineCreateInfo&);
		PipelineLayout createPipelineLayout(VkDevice, const VkPipelineLayoutCreateInfo&);
		RenderPass createRenderPass2(VkDevice, const VkRenderPassCreateInfo2&);
		Framebuffer createFramebuffer(VkDevice, const VkFramebufferCreateInfo&);
		CommandPool createCommandPool(VkDevice, const VkCommandPoolCreateInfo&);
		Semaphore createSemaphore(VkDevice, const VkSemaphoreCreateInfo&);
		DescriptorSetLayout createDescriptorSetLayout(VkDevice, const VkDescriptorSetLayoutCreateInfo&);
		DescriptorPool createDescriptorPool(VkDevice, const VkDescriptorPoolCreateInfo&);
		Sampler createSampler(VkDevice, const VkSamplerCreateInfo&);
		QueryPool createQueryPool(VkDevice, const VkQueryPoolCreateInfo&);

		CommandBufferArray allocateCommandBuffers(VkDevice, const VkCommandBufferAllocateInfo&);

		DebugUtilsMessengerEXT createDebugUtilsMessengerEXT(VkInstance, const VkDebugUtilsMessengerCreateInfoEXT&);
		SurfaceKHR createSurfaceKHR(VkInstance, VkSurfaceKHR) noexcept;
		SwapchainKHR createSwapchainKHR(VkDevice, const VkSwapchainCreateInfoKHR&);
		AccelerationStructureKHR createAccelerationStructureKHR(VkDevice, const VkAccelerationStructureCreateInfoKHR&);

	}

}

#undef DECLEAR_VULKAN_OBJECT_DELETER
#undef CREATE_VULKAN_OBJECT_ALIAS