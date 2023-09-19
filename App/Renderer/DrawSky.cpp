#include "DrawSky.hpp"

#include <LearnVulkan/GeneratedTemplate/ResourcePath.hpp>

#include "../Common/ErrorHandler.hpp"
#include "../Common/File.hpp"

#include "../Engine/Abstraction/BufferManager.hpp"
#include "../Engine/Abstraction/PipelineBarrier.hpp"
#include "../Engine/Abstraction/PipelineManager.hpp"
#include "../Engine/Abstraction/SemaphoreManager.hpp"
#include "../Engine/Abstraction/ShaderModuleManager.hpp"
#include "../Engine/IndirectCommand.hpp"

#include <shaderc/shaderc.h>

#include <array>
#include <string_view>
#include <numeric>

#include <cstring>

using std::array, std::string_view;
using std::ostream, std::endl;

using namespace LearnVulkan;
namespace VKO = VulkanObject;

namespace {

	constexpr IndirectCommand::VkDrawIndirectCommand SkyIndirect {
		3u,
		1u,
		0u,
		0u
	};

	/*****************
	 * Shader
	 ****************/
	constexpr string_view SkyVS = "/DrawSky.vert",
		SkyFS = "/DrawSky.frag";
	constexpr array SkyShaderKind = { shaderc_vertex_shader, shaderc_fragment_shader };

	constexpr auto SkyShaderFilenameRaw = File::toAbsolutePath<ResourcePath::ShaderRoot, SkyVS, SkyFS>();
	constexpr auto SkyShaderFilename = File::batchRawStringToView(SkyShaderFilenameRaw);

	/**************
	 * Setup
	 *************/
	inline ShaderModuleManager::ShaderOutputGenerator compileSkyShader(const VkDevice device, ostream& msg) {
		msg << "Compile sky shader" << endl;

		const ShaderModuleManager::ShaderBatchCompilationInfo sky_info {
			.Device = device,
			.ShaderFilename = SkyShaderFilename.data(),
			.ShaderKind = SkyShaderKind.data()
		};
		return ShaderModuleManager::batchShaderCompilation<SkyShaderKind.size()>(&sky_info, &msg);
	}

	inline VKO::DescriptorSetLayout createSkyDescriptorSetLayout(const VkDevice device) {
		constexpr static VkDescriptorSetLayoutBinding sky_binding {
			.binding = 0u,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 1u,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
		};
		return VKO::createDescriptorSetLayout(device, {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
			.bindingCount = 1u,
			.pBindings = &sky_binding
		});
	}

	template<size_t LayoutCount>
	inline VKO::PipelineLayout createSkyPipelineLayout(const VkDevice device,
		const array<VkDescriptorSetLayout, LayoutCount>& layout) {
		return VKO::createPipelineLayout(device, {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.setLayoutCount = static_cast<uint32_t>(LayoutCount),
			.pSetLayouts = layout.data()
		});
	}

	VKO::Pipeline createSkyPipeline(const VkDevice device, const VkPipelineLayout layout,
		ostream& msg, const DrawSky::DrawFormat& format) {
		const auto sky_shader_gen = compileSkyShader(device, msg);

		const auto [colour_format, depth_format, sample] = format;
		const VkPipelineRenderingCreateInfo sky_rendering {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = 1u,
			.pColorAttachmentFormats = &colour_format,
			.depthAttachmentFormat = depth_format
		};
		return PipelineManager::createSimpleGraphicsPipeline(device, layout, {
			.ShaderStage = sky_shader_gen.promise().ShaderStage,
			.Rendering = &sky_rendering,
			.PrimitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			.CullMode = VK_CULL_MODE_NONE,
			.Sample = sample,
			.Depth = {
				.Write = false,
				//See note in vertex shader to understand why we need the `or equal` part.
				//In fact `equal` comparison also works,
				//just to make sure there won't be spuriously failed test due to float precision problem.
				.Comparator = PipelineManager::DepthComparator::DefaultOrEqual
			}
		});
	}

	inline VKO::BufferAllocation createSkyIndirectCommandBuffer(const VkDevice device, const VmaAllocator allocator) {
		return BufferManager::createDeviceBuffer({
			device, allocator, sizeof(::SkyIndirect)
		}, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	}

}

DrawSky::DrawSky(const VulkanContext& ctx, const SkyCreateInfo& sky_info) :
	SkyIndirectCommand(createSkyIndirectCommandBuffer(ctx.Device, ctx.Allocator)),

	SkyShaderLayout(createSkyDescriptorSetLayout(this->getDevice())),
	PipelineLayout(createSkyPipelineLayout(this->getDevice(), array {
		sky_info.CameraDescriptorSetLayout,
		*this->SkyShaderLayout
	})),
	Pipeline(createSkyPipeline(this->getDevice(), this->PipelineLayout, *sky_info.DebugMessage, sky_info.OutputFormat)),
	
	SkyCommand(std::get<CommandBufferManager::InFlightCommandBufferArray>(
		CommandBufferManager::allocateCommandBuffer(ctx, VK_COMMAND_BUFFER_LEVEL_SECONDARY,
			CommandBufferManager::CommandBufferType::InFlight)
	)) {
	{
		const VKO::Semaphore sema = SemaphoreManager::createTimelineSemaphore(this->getDevice(), 0ull);
		const VKO::CommandBuffer cmd = VKO::allocateCommandBuffer(this->getDevice(), {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = ctx.CommandPool.Transient,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1u
		});
		CommandBufferManager::beginOneTimeSubmit(cmd);

		/****************************
		 * Prepare indirect command
		 ***************************/
		const VKO::BufferAllocation indirect_staging = BufferManager::createStagingBuffer({
			this->getDevice(), ctx.Allocator, sizeof(::SkyIndirect)
		}, BufferManager::HostAccessPattern::Sequential);

		VKO::MappedAllocation indirect_data = VKO::mapAllocation<IndirectCommand::VkDrawIndirectCommand>(ctx.Allocator, indirect_staging.first);
		*indirect_data = ::SkyIndirect;
		CHECK_VULKAN_ERROR(vmaFlushAllocation(ctx.Allocator, indirect_staging.first, 0ull, VK_WHOLE_SIZE));
		indirect_data.reset();

		BufferManager::recordCopyBuffer(indirect_staging.second, this->SkyIndirectCommand.second, cmd, sizeof(::SkyIndirect));

		/***************************
		 * Prepare cubemap texture
		 **************************/
		this->SkyBox.Image = ImageManager::createImageFromReadResult(cmd, *sky_info.Cubemap, {
			.Device = this->getDevice(),
			.Allocator = ctx.Allocator,
			.Flag = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
			.Level = 1u,
			.Usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			.Aspect = VK_IMAGE_ASPECT_COLOR_BIT
		});
		this->SkyBox.ImageView = ImageManager::createFullImageView({
			.Device = this->getDevice(),
			.Image = this->SkyBox.Image.second,
			.ViewType = VK_IMAGE_VIEW_TYPE_CUBE,
			.Format = sky_info.Cubemap->Format,
			.Aspect = VK_IMAGE_ASPECT_COLOR_BIT
		});
		this->SkyBox.Sampler = VKO::createSampler(this->getDevice(), {
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.magFilter = VK_FILTER_LINEAR,
			.minFilter = VK_FILTER_NEAREST,
			.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
			.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
			.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
			.addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
			.maxLod = VK_LOD_CLAMP_NONE
		});

		PipelineBarrier<0u, 1u, 1u> barrier;
		barrier.addBufferBarrier({
			VK_PIPELINE_STAGE_2_COPY_BIT,
			VK_ACCESS_2_TRANSFER_WRITE_BIT,
			VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
			VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT
		}, this->SkyIndirectCommand.second);
		barrier.addImageBarrier({
			VK_PIPELINE_STAGE_2_COPY_BIT,
			VK_ACCESS_2_TRANSFER_WRITE_BIT,
			VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
			VK_ACCESS_2_SHADER_SAMPLED_READ_BIT
		}, {
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		}, this->SkyBox.Image.second, ImageManager::createFullSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT));
		barrier.record(cmd);

		CHECK_VULKAN_ERROR(vkEndCommandBuffer(cmd));
		CommandBufferManager::submit<1u, 0u, 1u>({ this->getDevice(), ctx.Queue.Render }, { cmd }, {{ }},
			{{{ sema, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, 1ull }}});
		SemaphoreManager::wait<1u>(this->getDevice(), { }, {{{ sema, 1ull }}});
	}
	{
		const auto sky_ds_layout = array { *this->SkyShaderLayout };
		this->SkyShaderDescriptorBuffer = DescriptorBufferManager(ctx, sky_ds_layout,
			VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT);

		const VkDescriptorImageInfo skybox_image_info {
			.sampler = this->SkyBox.Sampler,
			.imageView = this->SkyBox.ImageView,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};

		using DU = DescriptorBufferManager::DescriptorUpdater;
		const DU sky_ds_updater = this->SkyShaderDescriptorBuffer.createUpdater(ctx);
		sky_ds_updater.update({
			.SetLayout = this->SkyShaderLayout,
			.SetIndex = 0u,
			.GetInfo = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, { .pCombinedImageSampler = &skybox_image_info } }
		});
	}
}

inline VkDevice DrawSky::getDevice() const noexcept {
	return this->SkyIndirectCommand.second->get_deleter().Device;
}

VkDescriptorImageInfo DrawSky::skyImageDescriptor() const noexcept {
	return {
		.sampler = this->SkyBox.Sampler,
		.imageView = this->SkyBox.ImageView,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	};
}

RendererInterface::DrawResult DrawSky::draw(const DrawInfo& draw_info) const {
	const auto [inherited_draw_info, fbo_input, depth_layout] = draw_info;
	const auto& [ctx, camera, delta_time, frame_idx, vp, draw_area, resolve_img, resolve_img_view] = *inherited_draw_info;

	const VkCommandBuffer cmd = this->SkyCommand[frame_idx];
	CommandBufferManager::beginOneTimeSubmitSecondary(cmd);

	/******************
	 * Dependencies
	 *****************/
	const FramebufferManager::PrepareFramebufferInfo prepare_info {
		.DepthLayout = depth_layout
	};
	const FramebufferManager::SubpassOutputDependencyIssueInfo dep_issue_info {
		.PrepareInfo = &prepare_info,
		.ResolveOutput = resolve_img
	};
	FramebufferManager::issueSubpassOutputDependency(cmd, *fbo_input, dep_issue_info);

	/******************
	 * Rendering
	 ******************/
	FramebufferManager::beginInitialRendering(cmd, *fbo_input, {
		.DependencyInfo = &dep_issue_info,
		.RenderArea = draw_area,
		.ResolveOutput = {
			.Colour = resolve_img_view
		},
		.RequiredAfterRendering = {
			.Colour = false,
			.Depth = false
		}
	});

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, this->Pipeline);
	vkCmdSetScissor(cmd, 0u, 1u, &draw_area);
	vkCmdSetViewport(cmd, 0u, 1u, &vp);

	/***************
	 * Descriptor
	 **************/
	const auto ds = array {
		camera->descriptorBufferBindingInfo(),
		VkDescriptorBufferBindingInfoEXT {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
			.address = BufferManager::addressOf(this->getDevice(), this->SkyShaderDescriptorBuffer.buffer()),
			.usage = VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT
		}
	};

	array<uint32_t, ds.size()> ds_idx;
	std::iota(ds_idx.begin(), ds_idx.end(), 0u);
	const auto offset = array {
		camera->descriptorBufferOffset(frame_idx),
		this->SkyShaderDescriptorBuffer.offset(0u)
	};

	vkCmdBindDescriptorBuffersEXT(cmd, static_cast<uint32_t>(ds.size()), ds.data());
	vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, this->PipelineLayout,
		0u, static_cast<uint32_t>(ds.size()), ds_idx.data(), offset.data());

	/**************
	 * Draw
	 **************/
	vkCmdDrawIndirect(cmd, this->SkyIndirectCommand.second, 0ull, 1u, 0u);
	vkCmdEndRendering(cmd);

	CHECK_VULKAN_ERROR(vkEndCommandBuffer(cmd));
	return {
		.DrawCommand = cmd,
		.WaitStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
	};
}