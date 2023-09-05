#include "ImageManager.hpp"
#include "PipelineBarrier.hpp"

#include "../../Common/ErrorHandler.hpp"
#include "../../VulkanLoader/stb_image.h"

#include <string>

#include <memory>
#include <utility>
#include <stdexcept>

#include <algorithm>
#include <ranges>
#include <execution>

#include <cstring>

using std::ranges::views::iota, std::ranges::transform;

using std::span;
using std::unique_ptr, std::make_pair;
using std::runtime_error;

using namespace LearnVulkan;
namespace VKO = VulkanObject;
using ImageManager::ImageBitWidth, ImageManager::ImageColourSpace;

#define EXPAND_IMAGE_READ_INFO const auto [channel, colour_space] = img_read_info
#define EXPAND_IMAGE_INFO const auto [device, allocator, img_type, format, extent, level, layer, sample, usage, init_layout] = image_info
#define EXPAND_IV_INFO const auto [device, image, view_type, format, aspect] = iv_info

namespace {

	struct FreeImage {

		inline void operator()(void* const img) const noexcept {
			stbi_image_free(img);
		}

	};
	//With automatic lifetime management.
	template<class Image_t>
	using StbImageData = unique_ptr<Image_t, FreeImage>;

#define DEFINE_COMMON_IMAGE_READ_CONFIG \
using HandleFormat = StbImageData<PixelFormat>
	/**
	 * @brief Configuration for individual image bit format.
	 * @tparam BitWidth Specify the bit width of pixel of the image to be read.
	*/
	template<ImageBitWidth BitWidth>
	struct ImageReadConfiguration;
	template<>
	struct ImageReadConfiguration<ImageBitWidth::Eight> {

		using PixelFormat = stbi_uc;
		constexpr static auto ReadFunction = stbi_load;

		DEFINE_COMMON_IMAGE_READ_CONFIG;

	};
	template<>
	struct ImageReadConfiguration<ImageBitWidth::Sixteen> {

		using PixelFormat = stbi_us;
		constexpr static auto ReadFunction = stbi_load_16;

		DEFINE_COMMON_IMAGE_READ_CONFIG;

	};
#undef DEFINE_COMMON_IMAGE_READ_CONFIG

	constexpr VmaAllocationCreateInfo CommonImageAllocationInfo {
		.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
		.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	};

	//Get the dimension of an image.
	VkExtent2D getImageInfo(const char* const filename) {
		int x, y, c;
		if (!stbi_info(filename, &x, &y, &c)) {
			using namespace std::string_literals;
			throw runtime_error("Cannot get the information of the image file \'"s + filename + "\'"s);
		}
		(void)c;
		return VkExtent2D(x, y);
	}

	template<ImageBitWidth IBW, class CFG = ::ImageReadConfiguration<IBW>>
	auto loadImage(const char* const filename, const int channel) {
		int x, y, c;
		typename CFG::PixelFormat* const pixel = CFG::ReadFunction(filename, &x, &y, &c, channel);
		(void)x;
		(void)y;
		(void)c;
		if (!pixel) {
			using namespace std::string_literals;
			throw runtime_error("Cannot load the image file with filename \'"s + filename + "\'"s);
		}

		return typename CFG::HandleFormat(pixel);
	}

	template<class TPixel>
	void convertRGBAToRG(const TPixel* const input_ptr, TPixel* const output_ptr, const VkExtent2D& dim) {
		const auto [w, h] = dim;
		const size_t pixel_count = w * h;

		const auto it = iota(size_t { 0 }, pixel_count);
		std::for_each(std::execution::par_unseq, it.begin(), it.end(), [input_ptr, output_ptr](const auto i) {
			const TPixel* const input_pixel = input_ptr + 4u * i;
			TPixel* const output_pixel = output_ptr + 2u * i;

			for (const auto p : iota(0u, 2u)) {
				output_pixel[p] = input_pixel[p];
			}
		});
	}

	template<ImageBitWidth BitWidth>
	VkFormat deduceImageFormat(ImageColourSpace, int);
	template<>
	VkFormat deduceImageFormat<ImageBitWidth::Eight>(const ImageColourSpace colour_space, const int channel) {
		constexpr static auto deduceLinearImageFormat8 = [](const int channel) -> VkFormat {
			switch (channel) {
			case 1:
				return VK_FORMAT_R8_UNORM;
			case 2:
				return VK_FORMAT_R8G8_UNORM;
			case 4:
				return VK_FORMAT_R8G8B8A8_UNORM;
			default:
				throw runtime_error("Cannot deduce the linear image format for 8-bit input given the channel count.");
			}
		};
		constexpr static auto deduceNonLinearImageFormat8 = [](const int channel) -> VkFormat {
			switch (channel) {
			case 1:
				return VK_FORMAT_R8_SRGB;
			case 2:
				return VK_FORMAT_R8G8_SRGB;
			case 4:
				return VK_FORMAT_R8G8B8A8_SRGB;
			default:
				throw runtime_error("Cannot deduce the non-linear image format for 8-bit input given the channel count.");
			}
		};

		using enum ImageColourSpace;
		switch (colour_space) {
		case Linear: return deduceLinearImageFormat8(channel);
		case SRGB: return deduceNonLinearImageFormat8(channel);
		default:
			throw runtime_error("The image colour space to deduce its image format is unknown.");
		}
	}
	template<>
	VkFormat deduceImageFormat<ImageBitWidth::Sixteen>(const ImageColourSpace colour_space, const int channel) {
		constexpr static auto deduceLinearImageFormat16 = [](const int channel) -> VkFormat {
			switch (channel) {
			case 1:
				return VK_FORMAT_R16_UNORM;
			case 4:
				return VK_FORMAT_R16G16B16A16_UNORM;
			default:
				throw runtime_error("Cannot deduce the linear image format for 16-bit input given the channel count.");
			}
		};

		using enum ImageColourSpace;
		switch (colour_space) {
		case Linear: return deduceLinearImageFormat16(channel);
		case SRGB: throw runtime_error("16-bit image does not support non-linear image format.");
		default:
			throw runtime_error("The image colour space to deduce its image format is unknown.");
		}
	}

	inline VkImageViewCreateInfo createCommonImageViewInfo(const ImageManager::ImageViewCreateInfo& iv_info,
		const VkImageSubresourceRange sub_res) noexcept {
		EXPAND_IV_INFO;
		return {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = image,
			.viewType = view_type,
			.format = format,
			.components = {
				VK_COMPONENT_SWIZZLE_IDENTITY,
				VK_COMPONENT_SWIZZLE_IDENTITY,
				VK_COMPONENT_SWIZZLE_IDENTITY,
				VK_COMPONENT_SWIZZLE_IDENTITY
			},
			.subresourceRange = sub_res
		};
	}

}

void ImageManager::_Internal::recordMipMapGeneration(const VkCommandBuffer cmd, const VkImage image,
	const ImageMipMapGenerationInfo& gen_info, const span<VkImageBlit2> blit_memory) {
	const auto [aspect, extent, layer_count] = gen_info;
	const uint32_t total_level = static_cast<uint32_t>(blit_memory.size());
	const int32_t w = extent.width,
		h = extent.height;
	int32_t mip_w = w,
		mip_h = h;

	constexpr static auto nextMipDim = [](const int32_t current) constexpr noexcept -> int32_t {
		return std::max(1, current / 2);
	};
	//most devices seem to have maximum texture size of 2^15, correspond to this many mip-level
	for (uint32_t level = 0u; level < total_level; level++) {
		//for simplicity we blit from level == 0 to each level > 0
		//so everything can be done concurrently without needing to issue a barrier in between
		mip_w = nextMipDim(mip_w);
		mip_h = nextMipDim(mip_h);
		blit_memory[level] = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
			.srcSubresource = ImageManager::createFullSubresourceLayers(aspect, 0u, layer_count),
			.srcOffsets = { { 0, 0, 0 }, { w, h, 1 } },
			.dstSubresource = ImageManager::createFullSubresourceLayers(aspect, level + 1u, layer_count),
			.dstOffsets = { { 0, 0, 0 }, { mip_w, mip_h, 1 } }
		};
	}
	const VkBlitImageInfo2 blit_info {
		.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
		.srcImage = image,
		.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		.dstImage = image,
		.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.regionCount = total_level,
		.pRegions = blit_memory.data()
	};
	vkCmdBlitImage2(cmd, &blit_info);
}

template<ImageBitWidth BitWidth>
ImageManager::ImageReadResult ImageManager::readFile(const VkDevice device, const VmaAllocator allocator,
	const span<const char* const> filename, const ImageReadInfo& img_read_info) {
	using CFG = ::ImageReadConfiguration<BitWidth>;
	EXPAND_IMAGE_READ_INFO;

	//need to first get the dimension of the image to allocate memory
	//we can assume all images have the same dimension and format
	const VkExtent2D dimension = ::getImageInfo(filename[0]);
	const auto [w, h] = dimension;

	const size_t layer_size = w * h * channel * sizeof(typename CFG::PixelFormat),
		total_size = layer_size * filename.size();
	VulkanObject::BufferAllocation staging = BufferManager::createStagingBuffer({ device, allocator, total_size },
		BufferManager::HostAccessPattern::Sequential);

	//laid out every layer contiguously
	void* data;
	CHECK_VULKAN_ERROR(vmaMapMemory(allocator, staging.first, &data));

	using std::as_const;
	const auto index = iota(size_t { 0 }, filename.size());
	std::ranges::for_each(index,
		[&filename = as_const(filename), buf = reinterpret_cast<uint8_t*>(data), channel, layer_size, &dimension](const auto i) {
		//what a shame stb_image does not allow use of custom allocator or user-provided memory
		//that can really save us from repeated allocation!

		//HACK: Since stb_image only allows loading RA channel if 2 channels are used,
		//but in fact we expect to use RG channel.
		//This can be done by first loading a RGBA channel, then swizzle the colour manually
		const bool require_rgba_to_rg = channel == 2;
		const int load_channel = require_rgba_to_rg ? 4 : channel;
		const typename CFG::HandleFormat pixel = ::loadImage<BitWidth>(filename[i], load_channel);

		const size_t offset = layer_size * i;//in byte
		auto* const output = reinterpret_cast<CFG::PixelFormat*>(buf + offset);
		if (require_rgba_to_rg) {
			convertRGBAToRG(pixel.get(), output, dimension);
		} else {
			std::memcpy(output, pixel.get(), layer_size);
		}
	});

	CHECK_VULKAN_ERROR(vmaFlushAllocation(allocator, staging.first, 0ull, total_size));
	vmaUnmapMemory(allocator, staging.first);

	return {
		.Extent = dimension,
		.Format = ::deduceImageFormat<BitWidth>(colour_space, channel),
		.Layer = static_cast<uint32_t>(filename.size()),
		.Pixel = std::move(staging)
	};
}

#define READ_FILE_INSTANTIATE(IBW) template ImageManager::ImageReadResult \
ImageManager::readFile<ImageBitWidth::IBW>(VkDevice, VmaAllocator, span<const char* const>, const ImageReadInfo&)

READ_FILE_INSTANTIATE(Eight);
READ_FILE_INSTANTIATE(Sixteen);

#undef READ_FILE_INSTANTIATE

VKO::ImageAllocation ImageManager::createImage(const ImageCreateInfo& image_info) {
	EXPAND_IMAGE_INFO;

	const VkImageCreateInfo img_info {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = img_type,
		.format = format,
		.extent = extent,
		.mipLevels = level,
		.arrayLayers = layer,
		.samples = sample,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = init_layout
	};
	return VKO::createImageFromAllocator(device, allocator, img_info, ::CommonImageAllocationInfo);
}

VKO::ImageAllocation ImageManager::createImageFromReadResult(const VkCommandBuffer cmd, const ImageReadResult& read_result,
	const ImageCreateFromReadResultInfo& image_read_result) {
	const auto& [extent, format, layer, pixel] = read_result;
	const auto [device, allocator, level, usage, aspect] = image_read_result;
	const auto [w, h] = extent;
	const VkExtent3D extent_3d = { w, h, 1u };

	VKO::ImageAllocation image = ImageManager::createImage({
		.Device = device,
		.Allocator = allocator,

		.ImageType = VK_IMAGE_TYPE_2D,
		.Format = format,
		.Extent = extent_3d,

		.Level = level,
		.Layer = layer,

		.Usage = usage
		});

	//we copy to level 0
	PipelineBarrier<0u, 0u, 1u> barrier;
	barrier.addImageBarrier({
		VK_PIPELINE_STAGE_2_NONE,
		VK_ACCESS_2_NONE,
		VK_PIPELINE_STAGE_2_COPY_BIT,
		VK_ACCESS_2_TRANSFER_WRITE_BIT
	}, {
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
	}, image.second, ImageManager::createEachLevelSubresourceRange(aspect, 0u));
	barrier.record(cmd);

	ImageManager::recordCopyImageFromBuffer(cmd, pixel.second, image.second, {
		.ImageExtent = extent_3d,
		//copy start from the first level and layer, and span across all layers in the base level
		.SubresourceLayers = ImageManager::createEachLayerSubresourceLayers(aspect, 0u, 0u)
	});

	return image;
}

void ImageManager::recordCopyImageFromBuffer(const VkCommandBuffer cmd, const VkBuffer source, const VkImage destination,
	const ImageCopyFromBufferInfo& copy_info) {
	const auto& [buffer_offset, image_offset, image_extent, buffer_row_length, buffer_image_height, sub_res_layer] = copy_info;

	const VkBufferImageCopy2 region {
		.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
		.bufferOffset = buffer_offset,
		.bufferRowLength = buffer_row_length,
		.bufferImageHeight = buffer_image_height,
		.imageSubresource = sub_res_layer,
		.imageOffset = image_offset,
		.imageExtent = image_extent
	};
	const VkCopyBufferToImageInfo2 buf_to_img {
		.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2,
		.srcBuffer = source,
		.dstImage = destination,
		.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.regionCount = 1u,
		.pRegions = &region
	};
	vkCmdCopyBufferToImage2(cmd, &buf_to_img);
}

void ImageManager::recordPrepareMipMapGeneration(const VkCommandBuffer cmd, const VkImage image,
	const ImagePrepareMipMapGenerationInfo& prep_info) {
	const auto [aspect, base_layout, base_stage, base_access] = prep_info;

	//we want to blit image from level 0 to all others, so transition their layouts accordingly
	PipelineBarrier<0u, 0u, 2u> barrier;
	//level == 0
	barrier.addImageBarrier({
		base_stage,
		base_access,
		VK_PIPELINE_STAGE_2_BLIT_BIT,
		VK_ACCESS_2_TRANSFER_READ_BIT
	}, {
		base_layout,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
	}, image, ImageManager::createEachLevelSubresourceRange(aspect, 0u));
	//level > 0
	VkImageSubresourceRange remaining_level = ImageManager::createFullSubresourceRange(aspect);
	remaining_level.baseMipLevel = 1u;
	barrier.addImageBarrier({
		VK_PIPELINE_STAGE_2_NONE,
		VK_ACCESS_2_NONE,
		VK_PIPELINE_STAGE_2_BLIT_BIT,
		VK_ACCESS_2_TRANSFER_WRITE_BIT
	}, {
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
	}, image, remaining_level);

	barrier.record(cmd);
}

void ImageManager::recordFinaliseMipMapGeneration(const VkCommandBuffer cmd, const VkImage image,
	const ImageFinaliseMipMapGenerationInfo& finalise_info) {
	const auto [aspect, target_layout, target_stage, target_access] = finalise_info;

	PipelineBarrier<0u, 0u, 2u> barrier;
	//level == 0
	barrier.addImageBarrier({
		VK_PIPELINE_STAGE_2_BLIT_BIT,
		VK_ACCESS_2_TRANSFER_READ_BIT,
		target_stage,
		target_access
	}, {
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		target_layout
	}, image, ImageManager::createEachLevelSubresourceRange(aspect, 0u));
	//level > 0
	VkImageSubresourceRange remaining_level = ImageManager::createFullSubresourceRange(aspect);
	remaining_level.baseMipLevel = 1u;
	barrier.addImageBarrier({
		VK_PIPELINE_STAGE_2_BLIT_BIT,
		VK_ACCESS_2_TRANSFER_WRITE_BIT,
		target_stage,
		target_access
	}, {
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		target_layout
	}, image, remaining_level);

	barrier.record(cmd);
}

VkImageSubresourceRange ImageManager::createFullSubresourceRange(const VkImageAspectFlags aspect) {
	return {
		.aspectMask = aspect,
		.baseMipLevel = 0u,
		.levelCount = VK_REMAINING_MIP_LEVELS,
		.baseArrayLayer = 0u,
		.layerCount = VK_REMAINING_ARRAY_LAYERS
	};
}

VkImageSubresourceLayers ImageManager::createFullSubresourceLayers(const VkImageAspectFlags aspect,
	const uint32_t level, const uint32_t layer_count) {
	return {
		.aspectMask = aspect,
		.mipLevel = level,
		.baseArrayLayer = 0u,
		.layerCount = layer_count
	};
}

VkImageSubresourceRange ImageManager::createEachLayerSubresourceRange(const VkImageAspectFlags aspect, const uint32_t layer) {
	return {
		.aspectMask = aspect,
		.baseMipLevel = 0u,
		.levelCount = VK_REMAINING_MIP_LEVELS,
		.baseArrayLayer = layer,
		.layerCount = 1u
	};
}

VkImageSubresourceRange ImageManager::createEachLevelSubresourceRange(const VkImageAspectFlags aspect, const uint32_t level) {
	return {
		.aspectMask = aspect,
		.baseMipLevel = level,
		.levelCount = 1u,
		.baseArrayLayer = 0u,
		.layerCount = VK_REMAINING_ARRAY_LAYERS
	};
}

VkImageSubresourceLayers ImageManager::createEachLayerSubresourceLayers(const VkImageAspectFlags aspect,
	const uint32_t level, const uint32_t layer) {
	return {
		.aspectMask = aspect,
		.mipLevel = level,
		.baseArrayLayer = layer,
		.layerCount = 1u
	};
}

VkImageSubresourceRange ImageManager::createEachLayerEachLevelSubresourceRange(const VkImageAspectFlags aspect,
	const uint32_t level, const uint32_t layer) {
	return {
		.aspectMask = aspect,
		.baseMipLevel = level,
		.levelCount = 1u,
		.baseArrayLayer = layer,
		.layerCount = 1u
	};
}

VKO::ImageView ImageManager::createFullImageView(const ImageViewCreateInfo& iv_info) {
	return VKO::createImageView(iv_info.Device, ::createCommonImageViewInfo(iv_info,
		ImageManager::createFullSubresourceRange(iv_info.Aspect)));
}

void ImageManager::createEachLayerImageView(const ImageViewCreateInfo& iv_info, const span<VKO::ImageView> output) {
	transform(iota(size_t { 0 }, output.size()), output.begin(), [&iv_info](const auto i) {
		return VKO::createImageView(iv_info.Device, ::createCommonImageViewInfo(iv_info,
			ImageManager::createEachLayerSubresourceRange(iv_info.Aspect, static_cast<uint32_t>(i))));
	});
}

VKO::Sampler ImageManager::createTextureSampler(const VkDevice device, const float anisotropy) {
	const VkSamplerCreateInfo sampler_info {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_LINEAR,
		.minFilter = VK_FILTER_LINEAR,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.anisotropyEnable = anisotropy > 0.0f,
		.maxAnisotropy = anisotropy,
		.maxLod = VK_LOD_CLAMP_NONE
	};
	return VKO::createSampler(device, sampler_info);
}