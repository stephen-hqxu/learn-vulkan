#include "SimpleWater.hpp"
#include "PlaneGeometry.hpp"

#include <LearnVulkan/GeneratedTemplate/ResourcePath.hpp>

#include "../Common/ErrorHandler.hpp"
#include "../Common/File.hpp"

#include "../Engine/Abstraction/BufferManager.hpp"
#include "../Engine/Abstraction/PipelineBarrier.hpp"
#include "../Engine/Abstraction/PipelineManager.hpp"
#include "../Engine/Abstraction/SemaphoreManager.hpp"
#include "../Engine/Abstraction/ShaderModuleManager.hpp"
#include "../Engine/EngineSetting.hpp"

#include <shaderc/shaderc.h>

#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/mat3x4.hpp>

#include <array>
#include <string_view>
#include <tuple>

#include <algorithm>
#include <ranges>
#include <numeric>
#include <cstring>

using glm::uvec2, glm::dvec2,
	glm::vec3;
using glm::mat4;

using std::array, std::string_view, std::tuple;
using std::views::iota, std::ranges::transform;
using std::ostream, std::endl;

using namespace LearnVulkan;
namespace VKO = VulkanObject;

namespace {

	constexpr double WaterNormalScale = 18.0,
		WaterAnimationSpeed = 0.02;

	struct WaterData {

		mat4 M;

		vec3 Tint = vec3(0.05f, 0.25f, 0.55f);
		float IoR = 1.0f / 1.333f,
			DoI = 158.8f,
			FS = 0.5f,
			AltOs = 278.5f,
			TD = 15.5f,
			NScl = ::WaterNormalScale,
			NStr = 0.2f,
			DisStr = 0.01f;

	};

	struct FragmentPushConstant {
		
		VkDeviceAddress V, I;
		float AniTim;

	};

	constexpr auto WaterDimension = dvec2(1755.5);
	constexpr auto WaterSubdivision = uvec2(8u);
	constexpr uint32_t WaterTextureMipMapCount = 6u;
	constexpr float WaterTextureAnisotropy = 5.5f;

	/******************
	 * Shader
	 ******************/
	constexpr string_view WaterVS = "/SimpleWater.vert",
		WaterFS = "/SimpleWater.frag";
	constexpr array WaterShaderKind = { shaderc_vertex_shader, shaderc_fragment_shader };

	constexpr auto WaterShaderFilenameRaw = File::toAbsolutePath<ResourcePath::ShaderRoot, WaterVS, WaterFS>();
	constexpr auto WaterShaderFilename = File::batchRawStringToView(WaterShaderFilenameRaw);

	/****************
	 * Setup
	 ****************/
	inline ShaderModuleManager::ShaderOutputGenerator compileWaterShader(const VkDevice device, ostream& out) {
		out << "Compiling water shader" << endl;

		const ShaderModuleManager::ShaderBatchCompilationInfo water_info {
			.Device = device,
			.ShaderFilename = WaterShaderFilename.data(),
			.ShaderKind = WaterShaderKind.data()
		};
		return ShaderModuleManager::batchShaderCompilation<WaterShaderFilename.size()>(&water_info, &out);
	}

	VKO::DescriptorSetLayout createWaterDescriptorSetLayout(const VkDevice device) {
		constexpr static size_t BindingCount = 4u;
		using WaterDSInfoEntry = tuple<VkDescriptorType, VkShaderStageFlags>;
		constexpr static auto water_ds_info = array<WaterDSInfoEntry, BindingCount> {
			tuple { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
			tuple { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_FRAGMENT_BIT },
			tuple { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT },
			tuple { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT }
		};

		array<VkDescriptorSetLayoutBinding, BindingCount> water_binding;
		transform(iota(size_t { 0 }, water_ds_info.size()), water_ds_info, water_binding.begin(),
			[](const auto i, const auto& info) constexpr noexcept {
			const auto [type, stage] = info;
			return VkDescriptorSetLayoutBinding {
				.binding = static_cast<uint32_t>(i),
				.descriptorType = type,
				.descriptorCount = 1u,
				.stageFlags = stage
			};
		});
		//texture samplers in fragment shader are arranged in an array
		water_binding[3].descriptorCount = 4u;

		return VKO::createDescriptorSetLayout(device, {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
			.bindingCount = static_cast<uint32_t>(water_binding.size()),
			.pBindings = water_binding.data()
		});
	}

	template<size_t LayoutCount>
	inline VKO::PipelineLayout createWaterPipelineLayout(const VkDevice device, const array<VkDescriptorSetLayout, LayoutCount>& layout) {
		constexpr static VkPushConstantRange water_pc {
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			.offset = 0u,
			.size = static_cast<uint32_t>(sizeof(::FragmentPushConstant))
		};
		return VKO::createPipelineLayout(device, {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.setLayoutCount = static_cast<uint32_t>(layout.size()),
			.pSetLayouts = layout.data(),
			.pushConstantRangeCount = 1u,
			.pPushConstantRanges = &water_pc
		});
	}

	VKO::Pipeline createWaterPipeline(const VkDevice device, VkPipelineLayout layout, ostream& out,
		const SimpleWater::DrawFormat& format) {
		const auto water_shader_gen = compileWaterShader(device, out);

		/////////////////////////
		/// Vertex input
		////////////////////////
		const auto [water_vertex_binding, water_vertex_attribute] = PlaneGeometry::vertexInput({
			.BindingIndex = 0u,
			.Location = {
				.Position = 0u,
				.UV = 1u
			}
		});
		const VkPipelineVertexInputStateCreateInfo water_vertex_input {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexBindingDescriptionCount = 1u,
			.pVertexBindingDescriptions = &water_vertex_binding,
			.vertexAttributeDescriptionCount = static_cast<uint32_t>(water_vertex_attribute.size()),
			.pVertexAttributeDescriptions = water_vertex_attribute.data()
		};

		//////////////////////
		/// Blending
		/////////////////////
		constexpr static array<VkPipelineColorBlendAttachmentState, 1u> water_blend {
			VkPipelineColorBlendAttachmentState {
				.blendEnable = VK_TRUE,
				.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
				.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
				.colorBlendOp = VK_BLEND_OP_ADD,
				.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
				.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
				.alphaBlendOp = VK_BLEND_OP_ADD,
				.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
			}
		};

		////////////////////
		/// Create pipeline
		////////////////////
		const auto [colour_format, depth_format, sample] = format;
		const VkPipelineRenderingCreateInfo water_rendering {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = 1u,
			.pColorAttachmentFormats = &colour_format,
			.depthAttachmentFormat = depth_format
		};
		return PipelineManager::createSimpleGraphicsPipeline(device, layout, {
			.ShaderStage = water_shader_gen.promise().ShaderStage,
			.VertexInputState = &water_vertex_input,
			.Rendering = &water_rendering,
			.PrimitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			.CullMode = VK_CULL_MODE_NONE,
			.Sample = sample,
			.Blending = water_blend
		});
	}

	inline VKO::BufferAllocation createWaterUniformBuffer(const VkDevice device, const VmaAllocator allocator) {
		return BufferManager::createDeviceBuffer({ device, allocator, sizeof(::WaterData) },
			VK_BUFFER_USAGE_TRANSFER_DST_BIT |
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_EXT
		);
	}

}

SimpleWater::SimpleWater(const VulkanContext& ctx, const WaterCreateInfo& water_info) :
	DepthFormat(water_info.OutputFormat.DepthFormat),
	UniformBuffer(createWaterUniformBuffer(ctx.Device, ctx.Allocator)),

	WaterShaderLayout(createWaterDescriptorSetLayout(this->getDevice())),
	PipelineLayout(createWaterPipelineLayout(this->getDevice(), array {
		water_info.CameraDescriptorSetLayout,
		*this->WaterShaderLayout
	})),
	Pipeline(createWaterPipeline(this->getDevice(), this->PipelineLayout, *water_info.DebugMessage, water_info.OutputFormat)),

	WaterCommand(std::get<CommandBufferManager::InFlightCommandBufferArray>(
		CommandBufferManager::allocateCommandBuffer(ctx, VK_COMMAND_BUFFER_LEVEL_SECONDARY,
			CommandBufferManager::CommandBufferType::InFlight)
	)),
	Animator(0.0) {
	{
		const VKO::Semaphore sema = SemaphoreManager::createTimelineSemaphore(this->getDevice(), 0ull);
		const VKO::CommandBuffer cmd = VKO::allocateCommandBuffer(this->getDevice(), {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = ctx.CommandPool.Transient,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1u
		});
		CommandBufferManager::beginOneTimeSubmit(cmd);

		//////////////////////////
		/// Generate water plane
		/////////////////////////
		{
			const VkCommandBuffer water_gen_cmd = water_info.PlaneGenerator->generate(ctx, {
				.Dimension = ::WaterDimension,
				.Subdivision = ::WaterSubdivision
			}, this->WaterSurface);
			vkCmdExecuteCommands(cmd, 1u, &water_gen_cmd);

			using enum GeometryData::BarrierTarget;
			this->WaterSurface.barrier(cmd, Generation, Rendering);
		}

		///////////////////////////
		/// Prepare shader uniform
		///////////////////////////
		const VKO::BufferAllocation water_data_staging = BufferManager::createStagingBuffer({
			this->getDevice(),
			this->getAllocator(),
			sizeof(::WaterData)
		}, BufferManager::HostAccessPattern::Sequential);

		VKO::MappedAllocation water_data = VKO::mapAllocation<::WaterData>(this->getAllocator(), water_data_staging.first);
		*water_data = {
			.M = *water_info.ModelMatrix
		};

		BufferManager::recordCopyBuffer(water_data_staging.second, this->UniformBuffer.second, cmd, sizeof(::WaterData));

		////////////////////////
		/// Build IAS
		////////////////////////
		using glm::mat3x4;
		static_assert(sizeof(VkTransformMatrixKHR) == sizeof(mat3x4));

		const VKO::BufferAllocation instance = BufferManager::createTransientHostBuffer({
			this->getDevice(),
			this->getAllocator(),
			sizeof(VkAccelerationStructureInstanceKHR)
		}, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			BufferManager::HostAccessPattern::Sequential);
		
		VKO::MappedAllocation instance_data = VKO::mapAllocation<VkAccelerationStructureInstanceKHR>(this->getAllocator(), instance.first);
		*instance_data = {
			.instanceCustomIndex = 0u,
			.mask = 0xFFu,
			.instanceShaderBindingTableRecordOffset = 0u,
			.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR | VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR,
			.accelerationStructureReference = AccelStructManager::addressOf(this->getDevice(), water_info.SceneGAS)
		};

		constexpr static auto identity = glm::identity<mat3x4>();
		//no need to transform from column-major to row-major, because the order of each number is the same
		std::memcpy(&instance_data->transform, glm::value_ptr(identity), sizeof(identity));

		const auto ias  = array {
			VkAccelerationStructureGeometryKHR {
				.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
				.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
				.geometry = {
					.instances = {
						.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
						.arrayOfPointers = VK_FALSE,
						.data = { .deviceAddress = BufferManager::addressOf(this->getDevice(), instance.second) }
					}
				},
				.flags = VK_GEOMETRY_OPAQUE_BIT_KHR
			}
		};
		constexpr static auto ias_range = array {
			VkAccelerationStructureBuildRangeInfoKHR {
				.primitiveCount = 1u,
				.primitiveOffset = 0u
				//the rests are ignored for IAS
			}
		};
		auto [accel_struct, scratch] = AccelStructManager::buildAccelStruct({
			.Device = this->getDevice(),
			.Allocator = this->getAllocator(),
			.Command = cmd,

			.Type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
			.Flag = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR
		}, ias, ias_range);
		this->SceneAccelStruct = std::move(accel_struct);

		/////////////////////////
		/// Create water texture
		////////////////////////
		const auto create_water_texture = [device = this->getDevice(), allocator = this->getAllocator(), cmd = *cmd]
			(const ImageManager::ImageReadResult& input, auto& output) -> void {
			output.Image = ImageManager::createImageFromReadResult(cmd, input, {
				.Device = device,
				.Allocator = allocator,
				.Level = ::WaterTextureMipMapCount,
				.Usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
				.Aspect = VK_IMAGE_ASPECT_COLOR_BIT
			});
			output.ImageView = ImageManager::createFullImageView({
				.Device = device,
				.Image = output.Image.second,
				.ViewType = VK_IMAGE_VIEW_TYPE_2D,
				.Format = input.Format,
				.Aspect = VK_IMAGE_ASPECT_COLOR_BIT
			});

			const auto [w, h] = input.Extent;
			ImageManager::recordFullMipMapGeneration<::WaterTextureMipMapCount>(cmd, output.Image.second, {
				.Aspect = VK_IMAGE_ASPECT_COLOR_BIT,
				.Extent = { w, h, 1u },
				.LayerCount = input.Layer,

				.InputStage = VK_PIPELINE_STAGE_2_COPY_BIT,
				.InputAccess = VK_ACCESS_2_TRANSFER_WRITE_BIT,
				.OutputStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
				.OutputAccess = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,

				.InputLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				.OutputLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			});
		};
		create_water_texture(*water_info.WaterNormalmap, this->Normalmap);
		create_water_texture(*water_info.WaterDistortion, this->Distortion);
		this->TextureSampler = ImageManager::createTextureSampler(this->getDevice(), ::WaterTextureAnisotropy);
		this->SceneDepthSampler = VKO::createSampler(this->getDevice(), {
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.magFilter = VK_FILTER_NEAREST,
			.minFilter = VK_FILTER_NEAREST,
			.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
			.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			.maxLod = VK_LOD_CLAMP_NONE
		});

		///////////////////
		/// Flush memory
		///////////////////
		const auto flush_allocation = array {
			*water_data_staging.first,
			*instance.first
		};
		CHECK_VULKAN_ERROR(vmaFlushAllocations(this->getAllocator(), static_cast<uint32_t>(flush_allocation.size()), flush_allocation.data(),
			nullptr, nullptr));

		instance_data.reset();
		water_data.reset();

		/////////////
		/// Barrier
		/////////////
		PipelineBarrier<0u, 2u, 0u> barrier;
		barrier.addBufferBarrier({
			VK_PIPELINE_STAGE_2_COPY_BIT,
			VK_ACCESS_2_TRANSFER_WRITE_BIT,
			VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
			VK_ACCESS_2_SHADER_STORAGE_READ_BIT
		}, this->UniformBuffer.second);
		barrier.addBufferBarrier({
			VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
			VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
			VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR
		}, this->SceneAccelStruct.AccelStructMemory.second);

		barrier.record(cmd);

		////////////////////
		/// Submission
		////////////////////
		CHECK_VULKAN_ERROR(vkEndCommandBuffer(cmd));
		CommandBufferManager::submit<1u, 0u, 1u>({ this->getDevice(), ctx.Queue.Render }, { cmd }, {{ }},
			{{{ sema, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, 1ull }}});
		SemaphoreManager::wait<1u>(this->getDevice(), { }, {{{ sema, 1ull }}});

		this->WaterSurface.releaseTemporary();
	}
	//////////////////////
	/// Descriptor buffer
	//////////////////////
	{
		const auto water_ds_layout = array { *this->WaterShaderLayout };
		this->WaterShaderDescriptorBuffer = DescriptorBufferManager(ctx, water_ds_layout,
			VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT);

		const VkDescriptorAddressInfoEXT water_data_addr {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT,
			.address = BufferManager::addressOf(this->getDevice(), this->UniformBuffer.second),
			.range = sizeof(::WaterData)
		};
		array<VkDescriptorImageInfo, 2u> water_texture_info;
		transform(array { *this->Normalmap.ImageView, *this->Distortion.ImageView }, water_texture_info.begin(),
			[sampler = *this->TextureSampler](const VkImageView iv) noexcept {
				return VkDescriptorImageInfo {
					.sampler = sampler,
					.imageView = iv,
					.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				};
			});

		using DU = DescriptorBufferManager::DescriptorUpdater;
		const DU water_ds_updater = this->WaterShaderDescriptorBuffer.createUpdater(ctx);

		//we update those non-array bindings first
		const VkDescriptorImageInfo env_map_info = water_info.SkyRenderer->skyImageDescriptor();
		const auto water_ds_get = array<DU::DescriptorGetInfo, 3u> {{
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, { .pStorageBuffer = &water_data_addr } },
			{ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, {
				.accelerationStructure = AccelStructManager::addressOf(this->getDevice(), this->SceneAccelStruct.AccelStruct) 
			}},
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, { .pCombinedImageSampler = &env_map_info } }
		}};
		for (const auto i : iota(0u, water_ds_get.size())) {
			water_ds_updater.update({
				.SetLayout = this->WaterShaderLayout,
				.SetIndex = 0u,
				.Binding = i,
				.GetInfo = water_ds_get[i]
			});
		}

		//samplers are arranged in an array in the same binding slot
		array<DU::DescriptorGetInfo, 3u> water_sampler_ds_get;
		std::ranges::for_each(water_sampler_ds_get, [](auto& get_info) constexpr noexcept {
			get_info.Type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		});
		water_sampler_ds_get[0].Data.pCombinedImageSampler = &water_info.SceneTexture;
		//there is one descriptor for scene depth texture, which will be initialised by reshape function
		transform(water_texture_info, water_sampler_ds_get.begin() + 1u, [](const auto& img_info) constexpr noexcept {
			return DU::DescriptorGetInfo {
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, { .pCombinedImageSampler = &img_info }
			};
		});
		
		for (const auto i : iota(0u, water_sampler_ds_get.size())) {
			water_ds_updater.update({
				.SetLayout = this->WaterShaderLayout,
				.SetIndex = 0u,
				.Binding = 3u,
				.ArrayLayer = i,
				.GetInfo = water_sampler_ds_get[i]
			});
		}
	}
}

inline VkDevice SimpleWater::getDevice() const noexcept {
	return this->UniformBuffer.second->get_deleter().Device;
}

inline VmaAllocator SimpleWater::getAllocator() const noexcept {
	return this->UniformBuffer.first->get_deleter().Allocator;
}

VkImageView SimpleWater::getSceneDepth() const noexcept {
	return this->SceneDepth.ImageView;
}

#define EXPAND_RECORD_INFO const auto [stage, access, layout] = record_info

void SimpleWater::beginSceneDepthRecord(const VkCommandBuffer cmd, const SceneDepthRecordInfo& record_info) const noexcept {
	EXPAND_RECORD_INFO;

	PipelineBarrier<0u, 0u, 1u> barrier;
	barrier.addImageBarrier({
		VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
		VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
		stage,
		access
	}, {
		VK_IMAGE_LAYOUT_UNDEFINED,
		layout
	}, this->SceneDepth.Image.second, ImageManager::createFullSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT));
	barrier.record(cmd);
}

void SimpleWater::endSceneDepthRecord(const VkCommandBuffer cmd, const SceneDepthRecordInfo& record_info) const noexcept {
	EXPAND_RECORD_INFO;

	PipelineBarrier<0u, 0u, 1u> barrier;
	barrier.addImageBarrier({
		stage,
		access,
		VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
		VK_ACCESS_2_SHADER_SAMPLED_READ_BIT
	}, {
		layout,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	}, this->SceneDepth.Image.second, ImageManager::createFullSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT));
	barrier.record(cmd);
}

#undef EXPAND_RECORD_INFO

void SimpleWater::reshape(const RendererInterface::ReshapeInfo& reshape_info) {
	const auto [ctx, extent] = reshape_info;

	auto& depth = this->SceneDepth;
	//ensure to destroy image view before image
	depth = { };

	const auto [w, h] = extent;
	depth.Image = ImageManager::createImage({
		.Device = this->getDevice(),
		.Allocator = this->getAllocator(),

		.ImageType = VK_IMAGE_TYPE_2D,
		.Format = this->DepthFormat,
		.Extent = { w, h, 1u },
		.Usage = VK_IMAGE_USAGE_SAMPLED_BIT
	});
	depth.ImageView = ImageManager::createFullImageView({
		.Device = this->getDevice(),
		.Image = depth.Image.second,
		.ViewType = VK_IMAGE_VIEW_TYPE_2D,
		.Format = this->DepthFormat,
		.Aspect = VK_IMAGE_ASPECT_DEPTH_BIT
	});

	const VkDescriptorImageInfo scene_depth_img {
		.sampler = this->SceneDepthSampler,
		.imageView = this->SceneDepth.ImageView,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	};

	const DescriptorBufferManager::DescriptorUpdater updater = this->WaterShaderDescriptorBuffer.createUpdater(*ctx);
	updater.update({
		.SetLayout = this->WaterShaderLayout,
		.SetIndex = 0u,
		.Binding = 3u,
		.ArrayLayer = 3u,
		.GetInfo = {
			.Type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.Data = { .pCombinedImageSampler = &scene_depth_img }
		}
	});
}

RendererInterface::DrawResult SimpleWater::draw(const DrawInfo& draw_info) const {
	const auto [inherited_draw_info, geometry, fbo_input, depth_layout] = draw_info;
	const auto& [ctx, camera, delta_time, frame_idx, vp, render_area, resolve_img, resolve_img_view] = *inherited_draw_info;

	const VkCommandBuffer cmd = this->WaterCommand[frame_idx];
	CommandBufferManager::beginOneTimeSubmitSecondary(cmd);

	/***********************
	 * Subpass dependencies
	 ***********************/
	const FramebufferManager::PrepareFramebufferInfo prepare_info {
		.DepthLayout = depth_layout
	};
	const FramebufferManager::SubpassOutputDependencyIssueInfo issue_info {
		.PrepareInfo = &prepare_info
	};
	FramebufferManager::issueSubpassOutputDependency(cmd, *fbo_input, issue_info);

	/*******************
	 * Rendering
	 ******************/
	//we will draw on the existing input framebuffer, so no need to clear
	FramebufferManager::beginInitialRendering(cmd, *fbo_input, {
		.DependencyInfo = &issue_info,
		.RenderArea = render_area
	});

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, this->Pipeline);
	vkCmdSetViewport(cmd, 0u, 1u, &vp);
	vkCmdSetScissor(cmd, 0u, 1u, &render_area);

	/********************
	 * Descriptor buffer
	 *******************/
	const auto ds = array {
		camera->descriptorBufferBindingInfo(),
		VkDescriptorBufferBindingInfoEXT {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
			.address = BufferManager::addressOf(this->getDevice(), this->WaterShaderDescriptorBuffer.buffer()),
			.usage = VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT
		}
	};

	array<uint32_t, ds.size()> ds_idx;
	std::iota(ds_idx.begin(), ds_idx.end(), 0u);
	const auto ds_offset = array {
		camera->descriptorBufferOffset(frame_idx),
		this->WaterShaderDescriptorBuffer.offset(0u)
	};

	vkCmdBindDescriptorBuffersEXT(cmd, static_cast<uint32_t>(ds.size()), ds.data());
	vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, this->PipelineLayout, 0u,
		static_cast<uint32_t>(ds.size()), ds_idx.data(), ds_offset.data());

	{
		this->Animator = glm::mod(this->Animator + delta_time * ::WaterAnimationSpeed, ::WaterNormalScale);

		const VkDeviceAddress geo_addr = BufferManager::addressOf(this->getDevice(), geometry->buffer());
		const auto [vertex_offset, index_offset, indirect_offset] = geometry->attributeInfo().Offset;
		const ::FragmentPushConstant frag_pc {
			geo_addr + vertex_offset,
			geo_addr + index_offset,
			static_cast<float>(this->Animator)
		};
		vkCmdPushConstants(cmd, this->PipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0u, sizeof(frag_pc), &frag_pc);
	}

	/*******************
	 * Vertex input
	 ******************/
	const GeometryData::AttributeInfo& attr_info = this->WaterSurface.attributeInfo();
	const auto [vertex_offset, index_offset, indirect_offset] = attr_info.Offset;
	const VkBuffer vbo = this->WaterSurface.buffer();

	vkCmdBindVertexBuffers(cmd, 0u, 1u, &vbo, &vertex_offset);
	vkCmdBindIndexBuffer(cmd, vbo, index_offset, attr_info.Type.Index);

	/********
	 * Draw
	 *******/
	vkCmdDrawIndexedIndirect(cmd, vbo, indirect_offset, 1u, 0u);
	vkCmdEndRendering(cmd);

	CHECK_VULKAN_ERROR(vkEndCommandBuffer(cmd));
	return {
		.DrawCommand = cmd,
		.WaitStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
	};
}