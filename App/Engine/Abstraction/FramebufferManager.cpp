#include "FramebufferManager.hpp"
#include "ImageManager.hpp"
#include "PipelineBarrier.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <ranges>
#include <utility>

#include <stdexcept>
#include <cstring>

using std::ranges::transform, std::ranges::views::iota;
using std::make_pair, std::move;
using std::array, std::optional;
using std::runtime_error;

using namespace LearnVulkan;
namespace VKO = VulkanObject;

#define EXPAND_PREPARE_INFO const auto [depth_layout] = prepare_info
#define EXPAND_ISSUE_INFO const auto [prepare_info_ptr, resolve_output] = issue_info

namespace {

	struct OutputAttachmentCreateInfo {

		VkDevice Device;
		VmaAllocator Allocator;

		VkExtent2D Extent;
		VkSampleCountFlagBits Sample;
		VkFormat Format;

		VkImageUsageFlags Usage;
		VkImageAspectFlags Aspect;

	};

	//Create a pair of layered image and an array of image views of each layer.
	auto createOutputAttachment(const OutputAttachmentCreateInfo& atm_info) {
		const auto [device, allocator, extent, sample, format, usage, aspect] = atm_info;
		const auto [w, h] = extent;

		VKO::ImageAllocation attachment = ImageManager::createImage({
			.Device = device,
			.Allocator = allocator,
			.ImageType = VK_IMAGE_TYPE_2D,
			.Format = format,
			.Extent = { w, h, 1u },
			.Sample = sample,
			.Usage = usage
		});
		VKO::ImageView attachment_view = ImageManager::createFullImageView({
			.Device = device,
			.Image = attachment.second,
			.ViewType = VK_IMAGE_VIEW_TYPE_2D,
			.Format = format,
			.Aspect = aspect
		});

		return make_pair(move(attachment), move(attachment_view));
	}

	VkImageAspectFlags deduceDepthStencilAspect(const VkFormat format) {
		constexpr static VkImageAspectFlags depth_only = VK_IMAGE_ASPECT_DEPTH_BIT,
			stencil_only = VK_IMAGE_ASPECT_STENCIL_BIT,
			depth_stencil = depth_only | stencil_only;
		switch (format) {
		case VK_FORMAT_D16_UNORM: [[fallthrough]];
		case VK_FORMAT_D32_SFLOAT: return depth_only;

		case VK_FORMAT_S8_UINT: return stencil_only;

		case VK_FORMAT_D16_UNORM_S8_UINT: [[fallthrough]];
		case VK_FORMAT_D24_UNORM_S8_UINT: [[fallthrough]];
		case VK_FORMAT_D32_SFLOAT_S8_UINT: return depth_stencil;

		default:
			throw runtime_error("Unable to deduce depth stencil aspect from the depth stencil format.");
		}
	}

}

FramebufferManager::SimpleFramebuffer FramebufferManager::createSimpleFramebuffer(const SimpleFramebufferCreateInfo& fbo_info) {
	const auto [device, allocator, colour_format, depth_format, sample, extent] = fbo_info;

	/**********************
	 * Create attachments
	 *********************/
	::OutputAttachmentCreateInfo atm_info = { device, allocator, extent, sample,
		colour_format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_ASPECT_COLOR_BIT };
	auto [colour_atm, colour_atm_view] = ::createOutputAttachment(atm_info);
	atm_info.Format = depth_format;
	atm_info.Usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	atm_info.Aspect = ::deduceDepthStencilAspect(depth_format);
	auto [depth_atm, depth_atm_view] = ::createOutputAttachment(atm_info);

	/***********************
	 * Move to output
	 **********************/
	return {
		move(colour_atm), move(depth_atm),
		{ move(colour_atm_view), move(depth_atm_view) }
	};
}

void FramebufferManager::prepareFramebuffer(const VkCommandBuffer cmd, const SimpleFramebuffer& fbo, const PrepareFramebufferInfo& prepare_info) {
	EXPAND_PREPARE_INFO;
	
	PipelineBarrier<0u, 0u, 2u> barrier;
	barrier.addImageBarrier({
		VK_PIPELINE_STAGE_2_NONE,
		VK_ACCESS_2_NONE,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
	}, {
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	}, fbo.Colour.second,
		ImageManager::createFullSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT));
	barrier.addImageBarrier({
		VK_PIPELINE_STAGE_2_NONE,
		VK_ACCESS_2_NONE,
		VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
		VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
	}, {
		VK_IMAGE_LAYOUT_UNDEFINED,
		depth_layout
	}, fbo.Depth.second,
		ImageManager::createFullSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT));
	barrier.record(cmd);
}

void FramebufferManager::issueSubpassOutputDependency(const VkCommandBuffer cmd, const SimpleFramebuffer& fbo,
	const SubpassOutputDependencyIssueInfo& issue_info) {
	EXPAND_ISSUE_INFO;
	const auto& prepare_info = *prepare_info_ptr;
	EXPAND_PREPARE_INFO;

	PipelineBarrier<0u, 0u, 3u> barrier;
	//basically we need to wait for a previous rendering that using the same attachment to finish
	barrier.addImageBarrier({
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
	}, {
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	}, fbo.Colour.second,
		ImageManager::createFullSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT));
	barrier.addImageBarrier({
		VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
		VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
		VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
	}, {
		depth_layout,
		depth_layout
	}, fbo.Depth.second,
		ImageManager::createFullSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT));

	//also issue a barrier for the resolve output image, if applicable
	if (resolve_output) {
		barrier.addImageBarrier({
			VK_PIPELINE_STAGE_2_NONE,
			VK_ACCESS_2_NONE,
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
		}, {
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		}, resolve_output,
			ImageManager::createFullSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT));
	}
	barrier.record(cmd);
}

void FramebufferManager::beginInitialRendering(const VkCommandBuffer cmd, const SimpleFramebuffer& fbo,
	const InitialRenderingBeginInfo& rendering_info) {
	static_assert(sizeof(VkClearColorValue) == sizeof(glm::vec4));

	const auto& [issue_info_ptr, colour, render_area, resolve_output_view, required_post_rendering] = rendering_info;
	const auto [resolve_colour, resolve_depth] = resolve_output_view;
	const auto& [store_colour, store_depth] = required_post_rendering;

	const auto& issue_info = *issue_info_ptr;
	EXPAND_ISSUE_INFO;
	const auto& prepare_info = *prepare_info_ptr;
	EXPAND_PREPARE_INFO;

	const auto& current_atm = fbo.Attachment;
	const bool should_clear = colour.has_value();
	const VkAttachmentLoadOp load_op = should_clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
	
	VkClearColorValue clear_colour = { };
	if (should_clear) {
		std::memcpy(&clear_colour.float32, glm::value_ptr(*colour), sizeof(VkClearColorValue));
	}

	const VkRenderingAttachmentInfo colour_atm {
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = current_atm.ColourView,
		.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.resolveMode = resolve_colour ? VK_RESOLVE_MODE_AVERAGE_BIT : VK_RESOLVE_MODE_NONE,
		.resolveImageView = resolve_colour,
		.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.loadOp = load_op,
		.storeOp = store_colour.value_or(!static_cast<bool>(resolve_colour)) ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.clearValue = { .color = clear_colour }
	};
	const VkRenderingAttachmentInfo depth_atm {
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = current_atm.DepthView,
		.imageLayout = depth_layout,
		.resolveMode = resolve_depth ? VK_RESOLVE_MODE_MAX_BIT : VK_RESOLVE_MODE_NONE,
		.resolveImageView = resolve_depth,
		.loadOp = load_op,
		.storeOp = store_depth.value_or(!static_cast<bool>(resolve_depth)) ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.clearValue = { .depthStencil = { 0.0f, 0u } }
	};
	const VkRenderingInfo vk_rendering_info {
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea = render_area,
		.layerCount = 1u,
		.colorAttachmentCount = 1u,
		.pColorAttachments = &colour_atm,
		.pDepthAttachment = &depth_atm
	};
	vkCmdBeginRendering(cmd, &vk_rendering_info);
}

void FramebufferManager::transitionAttachmentToPresent(const VkCommandBuffer cmd, const VkImage img) {
	PipelineBarrier<0u, 0u, 1u> barrier;
	barrier.addImageBarrier({
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		VK_PIPELINE_STAGE_2_NONE,
		VK_ACCESS_2_NONE
	}, {
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	}, img, ImageManager::createFullSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT));
	barrier.record(cmd);
}