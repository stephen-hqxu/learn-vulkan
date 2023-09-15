#include "DrawTriangle.hpp"

#include <LearnVulkan/GeneratedTemplate/ResourcePath.hpp>

#include "../Common/ErrorHandler.hpp"
#include "../Common/File.hpp"

#include "../Engine/Abstraction/BufferManager.hpp"
#include "../Engine/Abstraction/PipelineBarrier.hpp"
#include "../Engine/Abstraction/PipelineManager.hpp"
#include "../Engine/Abstraction/SemaphoreManager.hpp"
#include "../Engine/Abstraction/ShaderModuleManager.hpp"
#include "../Engine/EngineSetting.hpp"
#include "../Engine/IndirectCommand.hpp"

#include <shaderc/shaderc.h>

#include <string_view>
#include <array>
#include <initializer_list>
#include <utility>
#include <numeric>
#include <ranges>

#include <ostream>
#include <cstddef>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/trigonometric.hpp>
#include <glm/gtc/type_ptr.hpp>

using glm::u8vec2;
using glm::vec3, glm::dvec3, glm::i8vec3;
using glm::vec4, glm::dvec4;
using glm::mat4, glm::dmat4;

using std::array, std::string_view;
using std::ostream, std::endl;

using namespace LearnVulkan;
namespace VKO = VulkanObject;

namespace {

	constexpr double TriangleRotationSpeed = 0.1,/**< in radians per frame time */
		TriangleScale = 9.42;
	constexpr uint32_t TextureMipMapLevel = 6u;

	constexpr VkFormat ColourFormat = VK_FORMAT_R8G8B8A8_UNORM,
		DepthFormat = VK_FORMAT_D16_UNORM;
	constexpr VkSampleCountFlagBits TriangleMultiSample = VK_SAMPLE_COUNT_4_BIT;
	constexpr float TriangleMinSampleRate = 0.25f;

	/*******************
	 * Vertex / Index
	 *******************/
	struct TriangleVertex {

		i8vec3 Position;/**< 8-bit signed fixed point */
		u8vec2 UV;/**< 8-bit unsigned fixed point */

	};

	constexpr struct TriangleInput {

		constexpr static uint32_t IndexCount = 6u;

		TriangleVertex Vertex[4u] = {
			{{ -1, 0, -1 }, { 0u, 0u }},
			{{  1, 0, -1 }, { 1u, 0u }},
			{{  1, 0,  1 }, { 1u, 1u }},
			{{ -1, 0,  1 }, { 0u, 1u }}
		};

		uint8_t Index[IndexCount] = { 2u, 1u, 0u, 2u, 0u, 3u };

		IndirectCommand::VkDrawIndexedIndirectCommand Indirect = {
			TriangleInput::IndexCount,
			234u,
			0u,
			0,
			0u
		};

	} TriangleVertexIndexData = { };

	/********************
	 * Instance offset
	 ********************/
	//see vertex shader to see the variable meaning
	constexpr struct InstanceOffsetUniform {

		float A;
		float B, C;

	} InstanceOffsetData = { -1.5f, 35.5f, glm::radians(31.5f) };

	/*****************
	 * Shader
	 *****************/
	constexpr string_view TriangleVS = "/DrawTriangle.vert", TriangleFS = "/DrawTriangle.frag";
	constexpr array TriangleShaderKind = { shaderc_vertex_shader, shaderc_fragment_shader };

	constexpr auto TriangleShaderFilenameRaw = File::toAbsolutePath<ResourcePath::ShaderRoot, TriangleVS, TriangleFS>();
	constexpr auto TriangleShaderFilename = File::batchRawStringToView(TriangleShaderFilenameRaw);

	/***************
	 * Setup
	 ***************/
	inline ShaderModuleManager::ShaderOutputGenerator compileTriangleShader(const VkDevice device, ostream& out) {
		out << "Compiling triangle shader" << endl;
		
		const ShaderModuleManager::ShaderBatchCompilationInfo triangle_info {
			.Device = device,
			.ShaderFilename = TriangleShaderFilename.data(),
			.ShaderKind = TriangleShaderKind.data()
		};
		return ShaderModuleManager::batchShaderCompilation<TriangleShaderFilename.size()>(&triangle_info, &out);
	}

	template<size_t LayoutCount>
	inline VKO::PipelineLayout createTrianglePipelineLayout(const VkDevice device, const array<VkDescriptorSetLayout, LayoutCount>& ds_layout) {
		constexpr static VkPushConstantRange vertex_pc {
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
			.offset = 0u,
			.size = sizeof(mat4)
		};
		const VkPipelineLayoutCreateInfo triangle_layout {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.setLayoutCount = static_cast<uint32_t>(ds_layout.size()),
			.pSetLayouts = ds_layout.data(),
			.pushConstantRangeCount = 1u,
			.pPushConstantRanges = &vertex_pc
		};
		return VKO::createPipelineLayout(device, triangle_layout);
	}

	VKO::Pipeline createTriangleGraphicsPipeline(const VkDevice device, const VkPipelineLayout layout, ostream& out) {
		const auto triangle_shader_gen = compileTriangleShader(device, out);

		////////////////////////
		/// Vertex input state
		////////////////////////
		constexpr static VkVertexInputBindingDescription triangle_vertex_binding {
			.binding = 0u,
			.stride = sizeof(TriangleVertex),
			.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
		};
		constexpr static array triangle_vertex_attribute = {
			VkVertexInputAttributeDescription {
				.location = 0u,
				.binding = 0u,
				.format = VK_FORMAT_R8G8B8_SSCALED,
				.offset = offsetof(TriangleVertex, Position)
			},
			VkVertexInputAttributeDescription {
				.location = 1u,
				.binding = 0u,
				.format = VK_FORMAT_R8G8_USCALED,
				.offset = offsetof(TriangleVertex, UV)
			}
		};
		constexpr static VkPipelineVertexInputStateCreateInfo triangle_vertex_input {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexBindingDescriptionCount = 1u,
			.pVertexBindingDescriptions = &triangle_vertex_binding,
			.vertexAttributeDescriptionCount = static_cast<uint32_t>(triangle_vertex_attribute.size()),
			.pVertexAttributeDescriptions = triangle_vertex_attribute.data()
		};

		////////////////////////
		/// Rendering info
		///////////////////////
		constexpr static VkPipelineRenderingCreateInfo triangle_rendering {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = 1u,
			.pColorAttachmentFormats = &::ColourFormat,
			.depthAttachmentFormat = ::DepthFormat
		};

		return PipelineManager::createSimpleGraphicsPipeline(device, layout, {
			.ShaderStage = triangle_shader_gen.promise().ShaderStage,
			.VertexInputState = &triangle_vertex_input,
			.Rendering = &triangle_rendering,
			.PrimitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			.CullMode = VK_CULL_MODE_NONE,
			.Sample = ::TriangleMultiSample,
			.MinSampleShading = ::TriangleMinSampleRate
		});
	}

	inline VKO::BufferAllocation createTriangleBuffer(const VkDevice device, const VmaAllocator allocator) {
		return BufferManager::createDeviceBuffer({ device, allocator, sizeof(TriangleVertexIndexData) },
			VK_BUFFER_USAGE_TRANSFER_DST_BIT |
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
			VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
		);
	}

	inline VKO::DescriptorSetLayout createTriangleDescriptorSetLayout(const VkDevice device) {
		const array triangle_binding = {
			VkDescriptorSetLayoutBinding {
				.binding = 0u,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1u,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
			},
			VkDescriptorSetLayoutBinding {
				.binding = 1u,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.descriptorCount = 1u,
				.stageFlags = VK_SHADER_STAGE_VERTEX_BIT
			}
		};
		const VkDescriptorSetLayoutCreateInfo triangle_ds_layout {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
			.bindingCount = static_cast<uint32_t>(triangle_binding.size()),
			.pBindings = triangle_binding.data()
		};
		return VKO::createDescriptorSetLayout(device, triangle_ds_layout);
	}

	inline VKO::BufferAllocation createTriangleInstanceOffsetBuffer(const VkDevice device, const VmaAllocator allocator) {
		return BufferManager::createDeviceBuffer({ device, allocator, sizeof(InstanceOffsetData) },
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
		);
	}

}

DrawTriangle::DrawTriangle(const VulkanContext& ctx, const TriangleCreateInfo& triangle_info) :
	OutputExtent { },

	VertexBuffer(createTriangleBuffer(ctx.Device, ctx.Allocator)),
	VertexShaderInstanceOffset(createTriangleInstanceOffsetBuffer(this->getDevice(), this->getAllocator())),

	TriangleShaderLayout(createTriangleDescriptorSetLayout(this->getDevice())),

	PipelineLayout(createTrianglePipelineLayout(this->getDevice(), array { triangle_info.CameraDescriptorSetLayout, *this->TriangleShaderLayout })),
	Pipeline(createTriangleGraphicsPipeline(this->getDevice(), this->PipelineLayout, *triangle_info.DebugMessage)),

	TriangleDrawCmd(std::get<CommandBufferManager::InFlightCommandBufferArray>(
		CommandBufferManager::allocateCommandBuffer(ctx, VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			CommandBufferManager::CommandBufferType::InFlight)
	)),
	TriangleReshapeCmd(std::get<VKO::CommandBuffer>(
		CommandBufferManager::allocateCommandBuffer(ctx, VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			CommandBufferManager::CommandBufferType::Reshape)
	)),
	CurrentAngle(0.0) {
	//upload data using high performance staging buffer
	{
		const VKO::Semaphore copy_sema = SemaphoreManager::createTimelineSemaphore(this->getDevice(), 0ull);
		const VKO::CommandBuffer copy_cmd = VKO::allocateCommandBuffer(this->getDevice(), {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = ctx.CommandPool.Transient,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1u
		});
		CommandBufferManager::beginOneTimeSubmit(copy_cmd);

		/***********************
		 * Setup vertex buffer
		 **********************/
		const VKO::BufferAllocation vbo_staging = BufferManager::createStagingBuffer({
			this->getDevice(), this->getAllocator(), sizeof(TriangleVertexIndexData)
		}, BufferManager::HostAccessPattern::Sequential);

		VKO::MappedAllocation vbo_data = VKO::mapAllocation<::TriangleInput>(this->getAllocator(), vbo_staging.first);
		*vbo_data = TriangleVertexIndexData;

		/******************************
		 * Setup instance offset data
		 *****************************/
		const VKO::BufferAllocation instance_offset_staging = BufferManager::createStagingBuffer({
			this->getDevice(), this->getAllocator(), sizeof(InstanceOffsetData)
		}, BufferManager::HostAccessPattern::Sequential);

		VKO::MappedAllocation ins_off_data = VKO::mapAllocation<::InstanceOffsetUniform>(
			this->getAllocator(), instance_offset_staging.first);
		*ins_off_data = InstanceOffsetData;

		/****************************
		 * Transfer buffer data
		 ***************************/
		//flush both buffers together
		const array flush_alloc = { *vbo_staging.first, *instance_offset_staging.first };
		CHECK_VULKAN_ERROR(vmaFlushAllocations(this->getAllocator(), static_cast<uint32_t>(flush_alloc.size()),
			flush_alloc.data(), nullptr, nullptr));

		vbo_data.reset();
		ins_off_data.reset();

		BufferManager::recordCopyBuffer(vbo_staging.second, this->VertexBuffer.second, copy_cmd, sizeof(TriangleVertexIndexData));
		BufferManager::recordCopyBuffer(instance_offset_staging.second, this->VertexShaderInstanceOffset.second,
			copy_cmd, sizeof(InstanceOffsetData));

		//need some barriers to ensure copy has finished before rendering
		PipelineBarrier<0u, 2u, 0u> barrier;
		//vertex, index, indirect command
		barrier.addBufferBarrier({
			VK_PIPELINE_STAGE_2_COPY_BIT,
			VK_ACCESS_2_TRANSFER_WRITE_BIT,
			VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,
			VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_2_INDEX_READ_BIT | VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT
		}, this->VertexBuffer.second);
		//SSBO in vertex shader
		barrier.addBufferBarrier({
			VK_PIPELINE_STAGE_2_COPY_BIT,
			VK_ACCESS_2_TRANSFER_WRITE_BIT,
			VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
			VK_ACCESS_2_SHADER_STORAGE_READ_BIT
		}, this->VertexShaderInstanceOffset.second);
		barrier.record(copy_cmd);

		/************************
		 * Setup texture data
		 ************************/
		this->Texture.Image = ImageManager::createImageFromReadResult(copy_cmd, *triangle_info.SurfaceTexture, {
			this->getDevice(), this->getAllocator(), TextureMipMapLevel,
			VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_ASPECT_COLOR_BIT
		});
		this->Texture.ImageView = ImageManager::createFullImageView({
			this->getDevice(), this->Texture.Image.second, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT
		});
		this->Texture.Sampler = ImageManager::createTextureSampler(this->getDevice(), 14.5f);

		//generate mip-map
		const auto [w, h] = triangle_info.SurfaceTexture->Extent;
		ImageManager::recordFullMipMapGeneration<TextureMipMapLevel>(copy_cmd, this->Texture.Image.second, {
			.Aspect = VK_IMAGE_ASPECT_COLOR_BIT,
			.Extent = { w, h, 1u },
			.LayerCount = triangle_info.SurfaceTexture->Layer,

			.InputStage = VK_PIPELINE_STAGE_2_COPY_BIT,
			.InputAccess = VK_ACCESS_2_TRANSFER_WRITE_BIT,
			.OutputStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
			.OutputAccess = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,

			.InputLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.OutputLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		});

		/****************
		 * Submission
		 ****************/
		CHECK_VULKAN_ERROR(vkEndCommandBuffer(copy_cmd));
		//all we need is to wait for the host-to-device transfer to finish, before destroying the staging buffer
		//we can proceed safely while device-only operations are in-progress.
		CommandBufferManager::submit<1u, 0u, 1u>({ this->getDevice(), ctx.Queue.Render }, { copy_cmd }, {{ }},
			{{{ copy_sema, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, 1ull }}}, VK_NULL_HANDLE);
		SemaphoreManager::wait<1u>(this->getDevice(), { }, {{{ copy_sema, 1ull }}});
	}
	//allocate descriptor set
	{
		//we don't need to create one descriptor set for each in-flight frame because our data are not going to change
		const auto triangle_ds_layout = array { *this->TriangleShaderLayout };
		this->TriangleShaderDescriptorBuffer = DescriptorBufferManager(ctx, triangle_ds_layout,
			VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT);

		const VkDescriptorImageInfo triangle_shader_img_sampler {
			.sampler = this->Texture.Sampler,
			.imageView = this->Texture.ImageView,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};
		const VkDescriptorAddressInfoEXT triangle_shader_ssbo {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT,
			.address = BufferManager::addressOf(this->getDevice(), this->VertexShaderInstanceOffset.second),
			.range = sizeof(::InstanceOffsetUniform),
		};

		using DU = DescriptorBufferManager::DescriptorUpdater;
		const std::initializer_list<DU::DescriptorGetInfo> triangle_ds_update = {
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, { .pCombinedImageSampler = &triangle_shader_img_sampler } },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, { .pStorageBuffer = &triangle_shader_ssbo } }
		};

		const DU camera_ds_updater = this->TriangleShaderDescriptorBuffer.createUpdater(ctx);
		for (const auto i : std::views::iota(0u, triangle_ds_update.size())) {
			camera_ds_updater.update({
				.SetLayout = this->TriangleShaderLayout,
				.SetIndex = 0u,
				.Binding = i,
				.GetInfo = *(triangle_ds_update.begin() + i)
			});
		}
	}
}

inline VkDevice DrawTriangle::getDevice() const noexcept {
	return this->VertexBuffer.second->get_deleter().Device;
}

inline VmaAllocator DrawTriangle::getAllocator() const noexcept {
	return this->VertexBuffer.first->get_deleter().Allocator;
}

inline mat4 DrawTriangle::animateTriangle(const double delta) noexcept {
	constexpr static dvec3 yAxis = dvec3(0.0, 1.0, 0.0),
		xzAxis = dvec3(1.0, 0.0, 1.0);
	constexpr static double ResetFrequency = glm::pi<double>() * 2.0;
	this->CurrentAngle = glm::mod(this->CurrentAngle + delta * TriangleRotationSpeed, ResetFrequency);

	const dmat4 model = glm::scale(glm::identity<dmat4>(), xzAxis * TriangleScale + yAxis);//so y is always one
	return glm::rotate(model, this->CurrentAngle, yAxis);
}

void DrawTriangle::reshape(const ReshapeInfo& reshape_info) {
	const auto [ctx, extent] = reshape_info;

	this->OutputExtent = extent;
	this->OutputAttachment = FramebufferManager::createSimpleFramebuffer({
		.Device = this->getDevice(),
		.Allocator = this->getAllocator(),

		.ColourFormat = ::ColourFormat,
		.DepthFormat = ::DepthFormat,
		.Sample = ::TriangleMultiSample,

		.Extent = this->OutputExtent
	});

	const VkCommandBuffer cmd = this->TriangleReshapeCmd;
	CommandBufferManager::beginOneTimeSubmit(cmd);
	
	FramebufferManager::prepareFramebuffer(cmd, this->OutputAttachment, { VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL });

	CHECK_VULKAN_ERROR(vkEndCommandBuffer(cmd));
	CommandBufferManager::submit<1u>({ *ctx->Device, ctx->Queue.Render }, { cmd }, {{ }}, {{ }});
}

DrawTriangle::DrawResult DrawTriangle::draw(const DrawInfo& draw_info) {
	const auto& [ctx, camera, delta_time, frame_index, vp, draw_area, present_img, present_img_view] = draw_info;

	const VkCommandBuffer cmd = this->TriangleDrawCmd[frame_index];
	CommandBufferManager::beginOneTimeSubmit(cmd);

	/****************************
	 * Initial pipeline barrier
	 ****************************/
	constexpr static FramebufferManager::PrepareFramebufferInfo prepare_info {
		.DepthLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
	};
	const FramebufferManager::SubpassOutputDependencyIssueInfo issue_info {
		.PrepareInfo = &prepare_info,
		.ResolveOutput = present_img
	};
	FramebufferManager::issueSubpassOutputDependency(cmd, this->OutputAttachment, issue_info);
	
	/********************
	 * Setup rendering
	 ********************/
	constexpr static auto colour = vec4(vec3(25.5f) / 255.0f, 1.0f);
	FramebufferManager::beginInitialRendering(cmd, this->OutputAttachment, {
		.DependencyInfo = &issue_info,
		.ClearColour = colour,
		.RenderArea = draw_area,
		.ResolveOutput = {
			.Colour = present_img_view
		},
		.RequiredAfterRendering = {
			.Colour = false,
			.Depth = false
		}
	});

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, this->Pipeline);
	vkCmdSetViewport(cmd, 0u, 1u, &vp);
	vkCmdSetScissor(cmd, 0u, 1u, &draw_area);

	const mat4 model = this->animateTriangle(delta_time);
	vkCmdPushConstants(cmd, this->PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0u, sizeof(model), glm::value_ptr(model));

	/****************
	 * Descriptor
	 ****************/
	const auto ds = array {
		camera->descriptorBufferBindingInfo(),
		VkDescriptorBufferBindingInfoEXT {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
			.address = BufferManager::addressOf(this->getDevice(), this->TriangleShaderDescriptorBuffer.buffer()),
			.usage = VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT
		}
	};
	array<uint32_t, ds.size()> ds_idx;
	std::iota(ds_idx.begin(), ds_idx.end(), 0u);
	const auto ds_offset = array {
		camera->descriptorBufferOffset(frame_index),
		this->TriangleShaderDescriptorBuffer.offset(0u)
	};
	
	vkCmdBindDescriptorBuffersEXT(cmd, static_cast<uint32_t>(ds.size()), ds.data());
	vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, this->PipelineLayout, 0u,
		static_cast<uint32_t>(ds.size()), ds_idx.data(), ds_offset.data());

	/*******************
	 * Buffer binding
	 *******************/
	const VkBuffer vbo = *this->VertexBuffer.second;
	constexpr static VkDeviceSize vbo_offset = offsetof(TriangleInput, Vertex);

	vkCmdBindVertexBuffers(cmd, 0u, 1u, &vbo, &vbo_offset);
	vkCmdBindIndexBuffer(cmd, vbo, offsetof(TriangleInput, Index), VK_INDEX_TYPE_UINT8_EXT);

	/************
	 * Draw
	 ************/
	vkCmdDrawIndexedIndirect(cmd, vbo, offsetof(TriangleInput, Indirect), 1u, 0u);
	vkCmdEndRendering(cmd);

	/*************************
	 * Final pipeline barrier
	 ************************/
	FramebufferManager::transitionAttachmentToPresent(cmd, present_img);

	CHECK_VULKAN_ERROR(vkEndCommandBuffer(cmd));
	return {
		.DrawCommand = cmd,
		.WaitStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
	};
}