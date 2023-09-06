#pragma once

#include "BufferManager.hpp"

#include "../../Common/VulkanObject.hpp"

#include <array>
#include <span>
#include <cstdint>

namespace LearnVulkan {

	/**
	 * @brief A quick way of setting up an image for rendering.
	*/
	namespace ImageManager {

		enum class ImageColourSpace : uint8_t {
			Linear = 0x00u,
			SRGB = 0xFFu
		};

		enum class ImageBitWidth : uint8_t {
			Eight,/**< uint8_t */
			Sixteen/**< uint16_t */
		};

		struct ImageReadInfo {
		
			int Channel;/**< The number of channel to be loaded from the file. */
			ImageColourSpace ColourSpace;

		};

		struct ImageReadResult {
		
			VkExtent2D Extent;
			VkFormat Format;
			uint32_t Layer;

			VulkanObject::BufferAllocation Pixel;/**< Staging buffer, can be used to copy to an image straight away. */
		
		};

		struct ImageCreateInfo {

			VkDevice Device;
			VmaAllocator Allocator;

			VkImageType ImageType;
			VkFormat Format;
			VkExtent3D Extent;

			uint32_t Level = 1u, Layer = 1u;
			VkSampleCountFlagBits Sample = VK_SAMPLE_COUNT_1_BIT;

			VkImageUsageFlags Usage;
			VkImageLayout InitialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		};

		struct ImageCreateFromReadResultInfo {

			VkDevice Device;
			VmaAllocator Allocator;

			uint32_t Level;
			VkImageUsageFlags Usage;

			VkImageAspectFlags Aspect;

		};

		struct ImageCopyFromBufferInfo {

			VkDeviceSize BufferOffset = 0ull;
			VkOffset3D ImageOffset = { 0, 0, 0 };
			VkExtent3D ImageExtent;

			uint32_t BufferRowLength = 0u,
				BufferImageHeight = 0u;

			VkImageSubresourceLayers SubresourceLayers;

		};

		struct ImagePrepareMipMapGenerationInfo {

			VkImageAspectFlags Aspect;

			VkImageLayout BaseLevelLayout;
			VkPipelineStageFlags2 BaseLevelSourceStage;
			VkAccessFlags2 BaseLevelSourceAccess;

		};

		struct ImageMipMapGenerationInfo {
		
			VkImageAspectFlags Aspect;
			VkExtent3D Extent;
			uint32_t LayerCount;
		
		};

		struct ImageFinaliseMipMapGenerationInfo {

			VkImageAspectFlags Aspect;

			VkImageLayout TargetLayout;
			VkPipelineStageFlags2 TargetStage;
			VkAccessFlags2 TargetAccess;
		};

		struct ImageFullMipMapGenerationInfo {

			VkImageAspectFlags Aspect;
			VkExtent3D Extent;
			uint32_t LayerCount;

			//Stage and access flags for the base level.
			VkPipelineStageFlags2 InputStage;
			VkAccessFlags2 InputAccess;
			//Stage and access flags for the whole image.
			VkPipelineStageFlags2 OutputStage;
			VkAccessFlags2 OutputAccess;

			VkImageLayout InputLayout, OutputLayout;

		};

		struct ImageViewCreateInfo {
		
			VkDevice Device;
			VkImage Image;
			VkImageViewType ViewType;
			VkFormat Format;

			VkImageAspectFlags Aspect;
		
		};

		namespace _Internal {
		
			//Provide the memory for the blit info structure.
			//Must have the same size as the number of layer.
			void recordMipMapGeneration(VkCommandBuffer, VkImage, const ImageMipMapGenerationInfo&, std::span<VkImageBlit2>);
		
		}

		/**
		 * @brief Read an image from a file.
		 * @tparam BitWidth Specify the bit width of pixel of image to be read.
		 * @param device The device.
		 * @param allocator The allocator.
		 * @param filename An array of filename about the images.
		 * Each image will be laid out in memory contiguously in its order declared in this array.
		 * The behaviour is undefined if each image does not have the same size and format.
		 * @param img_read_info Additional information to read the image.
		 * @return The resulting data containing the image pixels.
		*/
		template<ImageBitWidth BitWidth>
		ImageReadResult readFile(VkDevice, VmaAllocator, std::span<const char* const>, const ImageReadInfo&);

		/**
		 * @brief Create an image.
		 * @param image_info The image creation info.
		 * @return The created image.
		*/
		VulkanObject::ImageAllocation createImage(const ImageCreateInfo&);

		/**
		 * @brief Create an image with data filled from a read result.
		 * @param cmd Command buffer to record command to copy the image from the read result.
		 * No pipeline barrier is provided.
		 * @param read_result The read result.
		 * @param image_read_result The information regarding how to create such image.
		 * @return The created image with the first level of each layer filled with data.
		*/
		VulkanObject::ImageAllocation createImageFromReadResult(VkCommandBuffer, const ImageReadResult&, const ImageCreateFromReadResultInfo&);

		/**
		 * @brief Record command to copy data from a buffer to image.
		 * After the copy, the image will have transfer destination optimal layout.
		 * @param cmd The command buffer.
		 * @param source The source buffer.
		 * @param destination The destination image.
		 * @param copy_info The information regarding the copy.
		*/
		void recordCopyImageFromBuffer(VkCommandBuffer, VkBuffer, VkImage, const ImageCopyFromBufferInfo&);

		/**
		 * @brief Record command to prepare an image for mip-map generation.
		 * Mip-map level > 0 will be generated from level == 0, and all level > 0 will be overwritten without barrier.
		 * Barrier will be issued to level == 0.
		 * @param cmd The command buffer.
		 * @param image The image to be prepared.
		 * @param prep_info Information to prepare the image.
		*/
		void recordPrepareMipMapGeneration(VkCommandBuffer, VkImage, const ImagePrepareMipMapGenerationInfo&);

		/**
		 * @brief Record command to generate mip-map for an image for all layers.
		 * @tparam TotalLevel The total number of mip level.
		 * @param cmd The command buffer.
		 * @param image The image to be have mip-map generated.
		 * @param gen_info Information for mip-map generation.
		*/
		template<size_t TotalLevel>
		inline void recordMipMapGeneration(const VkCommandBuffer cmd, const VkImage image, const ImageMipMapGenerationInfo& gen_info) {
			//we don't need to blit the first level, start from level 1
			std::array<VkImageBlit2, TotalLevel - 1u> blit_memory;
			_Internal::recordMipMapGeneration(cmd, image, gen_info, blit_memory);
		}

		/**
		 * @brief Record command to finish image mip-map generation.
		 * @param cmd The command buffer.
		 * @param image The image to be finalised.
		 * @param finalise_info Information to finalise the mip-map generation.
		*/
		void recordFinaliseMipMapGeneration(VkCommandBuffer, VkImage, const ImageFinaliseMipMapGenerationInfo&);

		/**
		 * @brief Record command to fully generate mip-map for an image for all layers.
		 * This is a one-liner function to prepare, generate and finalise mip-map generation.
		 * @tparam TotalLevel The total number of mip level.
		 * @param cmd The command buffer where generation commands will be recorded to.
		 * @param image The image whose mip-map will be generated.
		 * @param gen_info The generation info.
		*/
		template<size_t TotalLevel>
		inline void recordFullMipMapGeneration(const VkCommandBuffer cmd, const VkImage image, const ImageFullMipMapGenerationInfo& gen_info) {
			const auto [aspect, extent, layer, in_stage, in_access, out_stage, out_access, in_layout, out_layout] = gen_info;
			recordPrepareMipMapGeneration(cmd, image, {
				.Aspect = aspect,
				.BaseLevelLayout = in_layout,
				.BaseLevelSourceStage = in_stage,
				.BaseLevelSourceAccess = in_access
			});
			recordMipMapGeneration<TotalLevel>(cmd, image, {
				.Aspect = aspect,
				.Extent = extent,
				.LayerCount = layer
			});
			recordFinaliseMipMapGeneration(cmd, image, {
				.Aspect = aspect,
				.TargetLayout = out_layout,
				.TargetStage = out_stage,
				.TargetAccess = out_access
			});
		}

		//All layers and levels
		VkImageSubresourceRange createFullSubresourceRange(VkImageAspectFlags);
		//All layers and the provided level, also specifies layer count.
		VkImageSubresourceLayers createFullSubresourceLayers(VkImageAspectFlags, uint32_t, uint32_t);
		//All levels and the provided layer.
		VkImageSubresourceRange createEachLayerSubresourceRange(VkImageAspectFlags, uint32_t);
		//The provided level and all layers.
		VkImageSubresourceRange createEachLevelSubresourceRange(VkImageAspectFlags, uint32_t);
		//The provided level and layer, respectively.
		VkImageSubresourceLayers createEachLayerSubresourceLayers(VkImageAspectFlags, uint32_t, uint32_t);
		//The provided level and layer, respectively.
		VkImageSubresourceRange createEachLayerEachLevelSubresourceRange(VkImageAspectFlags, uint32_t, uint32_t);
	
		/**
		 * @brief Create an image view over the full memory range of an image.
		 * @param iv_info Information to create an image view.
		 * @param aspect The aspects of the image included into this view.
		 * @return The created full image view.
		*/
		VulkanObject::ImageView createFullImageView(const ImageViewCreateInfo&);

		/**
		 * @brief Create image views for each layer in the target image.
		 * @param iv_info Information to create an image view.
		 * @param output The output array of image views.
		*/
		void createEachLayerImageView(const ImageViewCreateInfo&, std::span<VulkanObject::ImageView>);

		/**
		 * @brief Create a sampler that is commonly used to sample texture in the shader.
		 * @param device The device.
		 * @param anisotropy The anisotropy filtering level.
		 * Specifying zero or negative to disable.
		 * @return The texture sampler.
		*/
		VulkanObject::Sampler createTextureSampler(VkDevice, float);
	
	}

}