#pragma once

#include "EngineSetting.hpp"
#include "../Common/VulkanObject.hpp"

#include <array>
#include <unordered_set>

#include <cstdint>

#ifdef LEARN_VULKAN_ENABLE_VALIDATION
#define CONTEXT_DISABLE_MESSAGE(RET_ID, CTX, MID) const auto RET_ID = (CTX).disableMessage(MID)
#define CONTEXT_ENABLE_MESSAGE(CTX, RET_ID) (CTX).enableMessage(RET_ID)
#else
#define CONTEXT_DISABLE_MESSAGE(...)
#define CONTEXT_ENABLE_MESSAGE(...)
#endif

namespace LearnVulkan {

	/**
	 * @brief A structure containing all necessary information regarding a renderer context,
	 * which can be easily shared with different renderers.
	*/
	class VulkanContext {
	private:

#ifdef LEARN_VULKAN_ENABLE_VALIDATION
		using MessageIDContainer = std::unordered_set<int32_t>;
		//Used by validation message control to ignore specific message.
		mutable MessageIDContainer IgnoredMessageID;
#endif

	public:

#ifdef LEARN_VULKAN_ENABLE_VALIDATION
		//The identifier returned
		using MessageIdentifier = MessageIDContainer::const_iterator;
#endif

		VulkanObject::Instance Instance;
		VkPhysicalDevice PhysicalDevice;

		VulkanObject::Device Device;
		VulkanObject::Allocator Allocator;
		struct {

			//This command pool does not allow individual command buffer reset,
			//and a pool-level reset is performed at the beginning of every in-flight frame.
			std::array<VulkanObject::CommandPool, EngineSetting::MaxFrameInFlight> InFlightCommandPool;

			//The reshape pool disallows individual command buffer reset, and will be reset at the beginning of every reshape event.
			VulkanObject::CommandPool Reshape,
				//The transient pool is optimised for temporary command buffer use, and does not allow reset.
				Transient,
				//The general pool allows individual reset.
				General;

		} CommandPool;

		struct {
		
			VkQueue Render, Present;
		
		} Queue;
		struct {
			
			uint32_t Render, Present;
		
		} QueueIndex;

		struct {
		
			VkPhysicalDeviceDescriptorBufferPropertiesEXT DescriptorBuffer;

		} PhysicalDeviceProperty;

		VulkanContext() noexcept = default;

		VulkanContext(VulkanContext&&) = delete;

		VulkanContext& operator=(VulkanContext&&) = delete;

		~VulkanContext() = default;

#ifdef LEARN_VULKAN_ENABLE_VALIDATION
		/**
		 * @brief Check if the message has been disabled.
		 * @param mid The message ID.
		 * @return True if the message has been disabled.
		*/
		inline bool isMessageDisabled(const int32_t mid) const {
			return this->IgnoredMessageID.contains(mid);
		}

		/**
		 * @brief Disable a message.
		 * @param mid The message ID.
		 * @return The identifier used to re-enable the message later.
		*/
		inline MessageIdentifier disableMessage(const int32_t mid) const {
			const auto [it, emplaced] = this->IgnoredMessageID.emplace(mid);
			return emplaced ? it : this->IgnoredMessageID.cend();
		}

		/**
		 * @brief Enable a message.
		 * @param id The identifier of the message to be enabled.
		*/
		inline void enableMessage(const MessageIdentifier id) const noexcept {
			if (id == this->IgnoredMessageID.cend()) {
				return;
			}
			this->IgnoredMessageID.erase(id);
		}
#endif

	};

}