#pragma once

#include "../../Common/StaticArray.hpp"
#include "../../Common/VulkanObject.hpp"

#include "../VulkanContext.hpp"

#include <span>
#include <vector>

#include <memory>
#include <cstdint>

namespace LearnVulkan {

	/**
	 * @brief A utility to create a descriptor buffer and issue bind command.
	*/
	class DescriptorBufferManager {
	public:

		/**
		 * @brief Descriptor updater is a utility to update descriptor information in a descriptor buffer.
		 * The update will not take effect until the descriptor updater is destroyed.
		*/
		class DescriptorUpdater {
		private:

			const VulkanContext* Context;
			DescriptorBufferManager* DesBufManager;

			VulkanObject::MappedAllocation<uint8_t> Mapped;

			/**
			 * @brief Flush descriptor buffer memory.
			*/
			struct DescriptorFlusher {

				DescriptorUpdater* Updater;

				void operator()(uint8_t*) const;

			};
			std::unique_ptr<uint8_t, DescriptorFlusher> Flusher;

		public:

			/**
			 * @brief The information regarding a descriptor.
			*/
			struct DescriptorGetInfo {

				VkDescriptorType Type;
				VkDescriptorDataEXT Data;

			};

			struct UpdateInfo {

				/**
				 * @brief The descriptor set layout at the index.
				 * This layout must be the same layout at the given set index as the layout used for initialisation.
				*/
				VkDescriptorSetLayout SetLayout;
			
				/**
				 * @brief The index into the descriptor set buffer.
				 * This should corresponds to the index into the descriptor set layout array initialising the current descriptor buffer instance.
				*/
				uint32_t SetIndex;
				/**
				 * @brief The binding location within the set.
				*/
				uint32_t Binding = 0u;
				/**
				 * @brief Specify the array layer index within the binding.
				*/
				uint32_t ArrayLayer = 0u;

				DescriptorGetInfo GetInfo;
			
			};

			/**
			 * @brief Initialise a descriptor buffer manager.
			 * @param ctx The context.
			 * @param dbm The parent descriptor buffer manager.
			*/
			DescriptorUpdater(const VulkanContext&, DescriptorBufferManager&);

			~DescriptorUpdater() = default;

			/**
			 * @brief Record an update.
			 * @param update_info The update info.
			*/
			void update(const UpdateInfo&) const;

		};

	private:

		VulkanObject::BufferAllocation DescriptorBuffer;
		StaticArray<VkDeviceSize> Offset;

		struct {

			std::vector<VmaAllocation> Allocation;
			std::vector<VkDeviceSize> Offset, Size;

		} Flush;
		bool UpdaterAlive;

		/**
		 * @brief Clear the temporary data in flush memory.
		*/
		constexpr void clearFlush() noexcept;

	public:

		/**
		 * @brief Initialise a descriptor buffer manager with no managed descriptor.
		 * Its state is undefined.
		*/
		DescriptorBufferManager() noexcept = default;

		/**
		 * @brief Initialise a descriptor buffer manager.
		 * @param ctx The context.
		 * @param ds_layout An array of descriptor set layout.
		 * @param usage The usage of the descriptor set buffer.
		 * A buffer device address usage is implicitly applied.
		*/
		DescriptorBufferManager(const VulkanContext&, std::span<const VkDescriptorSetLayout>, VkBufferUsageFlags);

		DescriptorBufferManager(DescriptorBufferManager&&) noexcept = default;

		DescriptorBufferManager& operator=(DescriptorBufferManager&&) noexcept = default;

		~DescriptorBufferManager() = default;

		/**
		 * @brief Create a descriptor updater.
		 * The current descriptor buffer manager must remain valid until the updater is destroyed.
		 * @param ctx The context.
		 * @return The descriptor updater.
		 * @exception It's only allowed to have one updater alive at a time.
		 * Exception is generated if another updater is created but not destroyed.
		*/
		DescriptorUpdater createUpdater(const VulkanContext&);

		/**
		 * @brief Get the descriptor set buffer.
		 * @return The descriptor set buffer.
		*/
		VkBuffer buffer() const noexcept;

		/**
		 * @brief Get an array of descriptor set offset.
		 * @return A span of descriptor set offset.
		*/
		std::span<const VkDeviceSize> offset() const noexcept;

		/**
		 * @brief Get the descriptor set offset given the index into the descriptor set in the buffer.
		 * @param index The index into the buffer. 
		 * @return The descriptor set offset.
		*/
		VkDeviceSize offset(size_t) const noexcept;

	};

}