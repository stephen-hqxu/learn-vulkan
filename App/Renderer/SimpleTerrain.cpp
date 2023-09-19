#include "SimpleTerrain.hpp"
#include "PlaneGeometry.hpp"

#include <LearnVulkan/GeneratedTemplate/ResourcePath.hpp>

#include "../Common/FixedArray.hpp"
#include "../Common/ErrorHandler.hpp"
#include "../Common/File.hpp"

#include "../Engine/Abstraction/BufferManager.hpp"
#include "../Engine/Abstraction/PipelineBarrier.hpp"
#include "../Engine/Abstraction/PipelineManager.hpp"
#include "../Engine/Abstraction/SemaphoreManager.hpp"
#include "../Engine/Abstraction/ShaderModuleManager.hpp"
#include "../Engine/EngineSetting.hpp"

#include <shaderc/shaderc.h>

#include <initializer_list>
#include <array>
#include <string_view>

#include <execution>
#include <numeric>
#include <algorithm>
#include <ranges>
#include <limits>

#include <stdexcept>

#include <glm/common.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <glm/mat3x4.hpp>
#include <glm/mat4x4.hpp>

using glm::uvec2, glm::vec2, glm::dvec2;
using glm::mat4;

using std::array, std::span, std::string_view;
using std::tuple, std::make_tuple;
using std::views::iota, std::ranges::transform;
using std::ostream, std::endl, std::runtime_error;

using namespace LearnVulkan;
namespace VKO = VulkanObject;

namespace {

	constexpr VkSampleCountFlagBits TerrainSampleCount = VK_SAMPLE_COUNT_4_BIT;
	constexpr VkFormat ColourFormat = VK_FORMAT_R8G8B8A8_UNORM,
		DepthFormat = VK_FORMAT_D32_SFLOAT;

	/*******************
	 * Uniform
	 ******************/
	constexpr struct TerrainUniform {

		struct {
			
			//remember GLSL uses column-major matrix
			mat4 M = mat4(
				1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 1.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				-655.5f, -333.3f, -655.5f, 1.0f
			);

		} TerrainTransform;
		alignas(float) struct {

			float A = 15.5f, B = 3.5f, C = 389.5f, D = 87.5f;

		} TessellationSetting;
		alignas(float) struct {

			float Alt = 455.5;

		} DisplacementSetting;

	} TerrainUniformData = { };

	constexpr auto TerrainSize = dvec2(1755.5);
	constexpr auto TerrainSubdivision = uvec2(20u),
		AccelStructTerrainSubdivision = uvec2(80u);

	/*********************
	 * Shader
	 *********************/
	constexpr string_view TerrainVS = "/SimpleTerrain.vert",
		TerrainTEC = "/SimpleTerrain.tesc", TerrainTEE = "/SimpleTerrain.tese", TerrainFS = "/SimpleTerrain.frag";
	constexpr array TerrainShaderKind = { shaderc_vertex_shader, shaderc_tess_control_shader,
		shaderc_tess_evaluation_shader, shaderc_fragment_shader };

	constexpr auto TerrainShaderFilenameRaw = File::toAbsolutePath<ResourcePath::ShaderRoot, TerrainVS, TerrainTEC, TerrainTEE, TerrainFS>();
	constexpr auto TerrainShaderFilename = File::batchRawStringToView(TerrainShaderFilenameRaw);

	/********************
	 * Setup
	 *******************/
	inline ShaderModuleManager::ShaderOutputGenerator compileTerrainShader(const VkDevice device, ostream& out) {
		out << "Compiling terrain shader" << endl;

		const ShaderModuleManager::ShaderBatchCompilationInfo terrain_info {
			.Device = device,
			.ShaderFilename = TerrainShaderFilename.data(),
			.ShaderKind = TerrainShaderKind.data()
		};
		return ShaderModuleManager::batchShaderCompilation<TerrainShaderFilename.size()>(&terrain_info, &out);
	}

	template<size_t LayoutCount>
	inline VKO::PipelineLayout createTerrainPipelineLayout(const VkDevice device, const array<VkDescriptorSetLayout, LayoutCount>& ds_layout) {
		const VkPipelineLayoutCreateInfo terrain_layout {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.setLayoutCount = static_cast<uint32_t>(ds_layout.size()),
			.pSetLayouts = ds_layout.data()
		};
		return VKO::createPipelineLayout(device, terrain_layout);
	}

	inline VKO::DescriptorSetLayout createTerrainDescriptorSetLayout(const VkDevice device) {
		constexpr static size_t BindingCount = 4u;
		constexpr static auto terrain_ds_info = array<tuple<VkDescriptorType, VkShaderStageFlags>, BindingCount> {
			tuple { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT },
			tuple { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT },
			tuple { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT },
			tuple { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
		};

		array<VkDescriptorSetLayoutBinding, BindingCount> terrain_binding;
		transform(iota(size_t { 0 }, terrain_binding.size()), terrain_ds_info, terrain_binding.begin(), [](const auto i, const auto& info) {
			const auto [type, shader] = info;
			return 	VkDescriptorSetLayoutBinding {
				.binding = static_cast<uint32_t>(i),
				.descriptorType = type,
				.descriptorCount = 1u,
				.stageFlags = shader
			};
		});
		
		const VkDescriptorSetLayoutCreateInfo terrain_ds_layout {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
			.bindingCount = static_cast<uint32_t>(BindingCount),
			.pBindings = terrain_binding.data()
		};
		return VKO::createDescriptorSetLayout(device, terrain_ds_layout);
	}

	VKO::Pipeline createTerrainGraphicsPipeline(const VkDevice device, const VkPipelineLayout layout, ostream& out) {
		const auto terrain_shader_gen = compileTerrainShader(device, out);

		////////////////////////////
		/// Vertex input state
		///////////////////////////
		const auto [terrain_vertex_binding, terrain_vertex_attribute] = PlaneGeometry::vertexInput({
			.BindingIndex = 0u,
			.Location = {
				.Position = 0u,
				.UV = 1u
			}
		});
		const VkPipelineVertexInputStateCreateInfo terrain_vertex_input {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexBindingDescriptionCount = 1u,
			.pVertexBindingDescriptions = &terrain_vertex_binding,
			.vertexAttributeDescriptionCount = static_cast<uint32_t>(terrain_vertex_attribute.size()),
			.pVertexAttributeDescriptions = terrain_vertex_attribute.data()
		};

		////////////////////
		/// Create pipeline
		////////////////////
		constexpr static VkPipelineRenderingCreateInfo terrain_rendering {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = 1u,
			.pColorAttachmentFormats = &::ColourFormat,
			.depthAttachmentFormat = ::DepthFormat
		};

		return PipelineManager::createSimpleGraphicsPipeline(device, layout, {
			.ShaderStage = terrain_shader_gen.promise().ShaderStage,
			.VertexInputState = &terrain_vertex_input,
			.Rendering = &terrain_rendering,
			.PrimitiveTopology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,
			.Sample = TerrainSampleCount
		});
	}

	inline VKO::BufferAllocation createTerrainUniformBuffer(const VkDevice device, const VmaAllocator allocator) {
		return BufferManager::createDeviceBuffer({ device, allocator, sizeof(::TerrainUniform) },
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
		);
	}

}

SimpleTerrain::SimpleTerrain(const VulkanContext& ctx, const TerrainCreateInfo& terrain_info) :
	OutputExtent { },

	UniformBuffer(createTerrainUniformBuffer(ctx.Device, ctx.Allocator)),
	
	TerrainShaderLayout(createTerrainDescriptorSetLayout(this->getDevice())),
	
	PipelineLayout(createTerrainPipelineLayout(this->getDevice(), array { terrain_info.CameraDescriptorSetLayout, *this->TerrainShaderLayout })),
	Pipeline(createTerrainGraphicsPipeline(this->getDevice(), this->PipelineLayout, *terrain_info.DebugMessage)),
	
	TerrainDrawCmd(std::get<CommandBufferManager::InFlightCommandBufferArray>(
		CommandBufferManager::allocateCommandBuffer(ctx, VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			CommandBufferManager::CommandBufferType::InFlight)
	)),
	TerrainReshapeCmd(std::get<VKO::CommandBuffer>(
		CommandBufferManager::allocateCommandBuffer(ctx, VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			CommandBufferManager::CommandBufferType::Reshape)
	)),
	
	SkyRenderer(ctx, DrawSky::SkyCreateInfo {
		.CameraDescriptorSetLayout = terrain_info.CameraDescriptorSetLayout,
		.OutputFormat = {
			.ColourFormat = ::ColourFormat,
			.DepthFormat = ::DepthFormat,
			.Sample = ::TerrainSampleCount
		},
		.Cubemap = terrain_info.SkyInfo->SkyBox,
		.DebugMessage = terrain_info.DebugMessage
	}) {
	//needs to ensure the plane generator survives until generation is complete
	const auto plane_generator = PlaneGeometry(ctx, *terrain_info.DebugMessage);
	const bool render_water = terrain_info.WaterInfo != nullptr;

	VKO::QueryPool accel_struct_query;
	const VKO::Semaphore copy_sema = SemaphoreManager::createTimelineSemaphore(this->getDevice(), 0ull);
	const VKO::CommandBufferArray cmd_array = VKO::allocateCommandBuffers(this->getDevice(), {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = ctx.CommandPool.Transient,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 2u
	});
	const VkCommandBuffer copy_cmd = cmd_array[0],
		compact_cmd = cmd_array[1];

	const auto submit_command = [device = this->getDevice(), queue = ctx.Queue.Render, sema = *copy_sema]
		(const VkCommandBuffer cmd, const uint32_t timeline) {
		CHECK_VULKAN_ERROR(vkEndCommandBuffer(cmd));
		CommandBufferManager::submit<1u, 0u, 1u>({ device, queue }, { cmd }, {{ }},
			{{{ sema, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, timeline }}}, VK_NULL_HANDLE);
		SemaphoreManager::wait<1u>(device, { }, {{{ sema, timeline }}});
	};

	{
		CommandBufferManager::beginOneTimeSubmit(copy_cmd);

		/**********************
		 * Prepare uniform
		 *********************/
		const VKO::BufferAllocation uniform_staging = BufferManager::createStagingBuffer(
			{ this->getDevice(), this->getAllocator(), sizeof(TerrainUniformData) }, BufferManager::HostAccessPattern::Sequential);

		VKO::MappedAllocation uni_data = VKO::mapAllocation<::TerrainUniform>(this->getAllocator(), uniform_staging.first);
		*uni_data = TerrainUniformData;

		/****************
		 * Transfer
		 ****************/
		CHECK_VULKAN_ERROR(vmaFlushAllocation(this->getAllocator(), uniform_staging.first, VkDeviceSize { 0 }, VK_WHOLE_SIZE));
		uni_data.reset();

		BufferManager::recordCopyBuffer(uniform_staging.second, this->UniformBuffer.second, copy_cmd, sizeof(::TerrainUniformData));

		/**********************
		 * Prepare terrain map
		 **********************/
		const ImageManager::ImageCreateFromReadResultInfo terrain_map_read_info {
			.Device = this->getDevice(),
			.Allocator = this->getAllocator(),
			//it's just a heightmap and normalmap, we don't need mipmaps for this
			.Level = 1u,
			.Usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			.Aspect = VK_IMAGE_ASPECT_COLOR_BIT
		};
		this->Heightfield.Image = ImageManager::createImageFromReadResult(copy_cmd, *terrain_info.Heightfield, terrain_map_read_info);
		
		ImageManager::ImageViewCreateInfo heightfield_img_view_info {
			.Device = this->getDevice(),
			.Image = this->Heightfield.Image.second,
			.ViewType = VK_IMAGE_VIEW_TYPE_2D,
			.Format = terrain_info.Heightfield->Format,
			.Aspect = VK_IMAGE_ASPECT_COLOR_BIT
		};
		this->Heightfield.FullView = ImageManager::createFullImageView(heightfield_img_view_info);
		heightfield_img_view_info.ComponentMapping = {
			//alpha channel is where our displacement info is stored
			VK_COMPONENT_SWIZZLE_A,
			VK_COMPONENT_SWIZZLE_ZERO,
			VK_COMPONENT_SWIZZLE_ZERO,
			VK_COMPONENT_SWIZZLE_ONE
		};
		this->Heightfield.DisplacementSwizzleView = ImageManager::createFullImageView(heightfield_img_view_info);
		heightfield_img_view_info.ComponentMapping = {
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_ONE
		};
		this->Heightfield.NormalOnlyView = ImageManager::createFullImageView(heightfield_img_view_info);

		constexpr static VkSamplerCreateInfo texture_sampler_info {
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.magFilter = VK_FILTER_LINEAR,
			.minFilter = VK_FILTER_NEAREST,
			.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
			.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			.maxLod = VK_LOD_CLAMP_NONE
		};
		this->Heightfield.Sampler = VKO::createSampler(this->getDevice(), texture_sampler_info);

		/*********************
		 * Barrier
		 ********************/
		PipelineBarrier<0u, 1u, 1u> barrier;
		//uniform data
		barrier.addBufferBarrier({
			VK_PIPELINE_STAGE_2_COPY_BIT,
			VK_ACCESS_2_TRANSFER_WRITE_BIT,
			VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT
			| VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT
			| VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT
			| VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
			VK_ACCESS_2_SHADER_STORAGE_READ_BIT
		}, this->UniformBuffer.second);
		
		const VkImageSubresourceRange full_image = ImageManager::createFullSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
		//heightfield
		barrier.addImageBarrier({
			VK_PIPELINE_STAGE_2_COPY_BIT,
			VK_ACCESS_2_TRANSFER_WRITE_BIT,
			//we will be using heightmap when displacing the plane later
			//EBI: optionally include displacement stage and access flags only when we need to perform a displacement computation
			VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT
				| PlaneGeometry::DisplacementStage | SimpleWater::WaterCreateInfo::TextureStage,
			VK_ACCESS_2_SHADER_SAMPLED_READ_BIT
				| PlaneGeometry::DisplacementAccess | SimpleWater::WaterCreateInfo::TextureAccess
		}, {
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		}, this->Heightfield.Image.second, full_image);
		barrier.record(copy_cmd);

		/****************************
		 * Prepare terrain geometry
		 ***************************/
		//this memory needs to be preserved until all commands are finished by the device
		AccelStructBuildTempMemory as_temp_mem;
		{
			///////////////
			/// Generation
			///////////////
			FixedArray<VkCommandBuffer, 2u> subcommand;

			subcommand.pushBack(plane_generator.generate(ctx, {
				.Dimension = ::TerrainSize,
				.Subdivision = ::TerrainSubdivision
			}, this->Plane));
			if (render_water) {
				//generate a plane for water scene acceleration structure using different LoD
				subcommand.pushBack(plane_generator.generate(ctx, {
					.Dimension = ::TerrainSize,
					.Subdivision = ::AccelStructTerrainSubdivision,
					.RequireAccelStructInput = true
				}, this->AccelStructPlane));
			}

			vkCmdExecuteCommands(copy_cmd, static_cast<uint32_t>(subcommand.size()), subcommand.data());

			//////////////////////
			/// Prepare for water
			//////////////////////
			using enum GeometryData::BarrierTarget;
			this->Plane.barrier(copy_cmd, Generation, Rendering);
			
			if (render_water) {
				this->AccelStructPlane.barrier(copy_cmd, Generation, Displacement);

				const VkCommandBuffer disp_cmd = plane_generator.displace(ctx, {
					.Altitude = ::TerrainUniformData.DisplacementSetting.Alt,
					.DisplacementMap = {
						.sampler = this->Heightfield.Sampler,
						.imageView = this->Heightfield.DisplacementSwizzleView,
						.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
					}
				}, this->AccelStructPlane);
				vkCmdExecuteCommands(copy_cmd, 1u, &disp_cmd);

				this->AccelStructPlane.barrier(copy_cmd, Displacement, AccelStructBuild);

				accel_struct_query = VKO::createQueryPool(this->getDevice(), {
					.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
					.queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
					.queryCount = 1u
				});
				vkCmdResetQueryPool(copy_cmd, accel_struct_query, 0u, 1u);
				as_temp_mem = this->buildTerrainAccelStruct(ctx, copy_cmd, accel_struct_query);
			}
		}

		/*********************
		 * Submission
		 ********************/
		submit_command(copy_cmd, 1u);
		this->Plane.releaseTemporary();
		if (render_water) {
			this->AccelStructPlane.releaseTemporary();
		}
	}
	if (render_water) {
		/***********************
		 * Compact GAS
		 **********************/
		CommandBufferManager::beginOneTimeSubmit(compact_cmd);
		AccelStructManager::AccelStruct compacted_as = this->compactTerrainAccelStruct(compact_cmd, accel_struct_query);
		submit_command(compact_cmd, 2u);
		
		this->TerrainAccelStruct = std::move(compacted_as);

		/*****************************
		 * Initialise water renderer
		 ****************************/
		this->WaterRenderer.emplace(ctx, SimpleWater::WaterCreateInfo {
			.CameraDescriptorSetLayout = terrain_info.CameraDescriptorSetLayout,
			.OutputFormat = {
				.ColourFormat = ::ColourFormat,
				.DepthFormat = ::DepthFormat,
				.Sample = ::TerrainSampleCount
			},

			.SkyRenderer = &this->SkyRenderer,
			.PlaneGenerator = &plane_generator,
			.SceneGAS = this->TerrainAccelStruct.AccelStruct,
			.SceneTexture = {
				.sampler = this->Heightfield.Sampler,
				.imageView = this->Heightfield.NormalOnlyView,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			},

			.WaterNormalmap = terrain_info.WaterInfo->WaterNormalmap,
			.WaterDistortion = terrain_info.WaterInfo->WaterDistortion,

			.ModelMatrix = &::TerrainUniformData.TerrainTransform.M,

			.DebugMessage = terrain_info.DebugMessage
		});
	}
	{
		const auto terrain_ds_layout = array { *this->TerrainShaderLayout };
		this->TerrainShaderDescriptorBuffer = DescriptorBufferManager(ctx, terrain_ds_layout,
			VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT);

		const auto map_ds = array {
			VkDescriptorImageInfo {
				.sampler = this->Heightfield.Sampler,
				.imageView = this->Heightfield.FullView,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			}
		};

		const VkDeviceAddress uniform_addr = BufferManager::addressOf(this->getDevice(), this->UniformBuffer.second);
		const std::initializer_list<tuple<VkDeviceAddress, VkDeviceSize>> terrain_uniform {
			{ uniform_addr + offsetof(::TerrainUniform, TerrainTransform), sizeof(::TerrainUniform::TerrainTransform) },
			{ uniform_addr + offsetof(::TerrainUniform, TessellationSetting), sizeof(::TerrainUniform::TessellationSetting) },
			{ uniform_addr + offsetof(::TerrainUniform, DisplacementSetting), sizeof(::TerrainUniform::DisplacementSetting) }
		};
		array<VkDescriptorAddressInfoEXT, 3u> uniform_ds;
		transform(terrain_uniform, uniform_ds.begin(), [](const auto& info) {
			const auto [addr, range] = info;
			return VkDescriptorAddressInfoEXT {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT,
				.address = addr,
				.range = range
			};
		});

		using DU = DescriptorBufferManager::DescriptorUpdater;
		array<DU::DescriptorGetInfo, uniform_ds.size() + map_ds.size()> terrain_ds_update;
		const auto terrain_sample_update = transform(uniform_ds, terrain_ds_update.begin(), [](const auto& info) {
			return DU::DescriptorGetInfo { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, { .pStorageBuffer = &info } };
		}).out;
		transform(map_ds, terrain_sample_update, [](const auto& sampler_info) {
			return DU::DescriptorGetInfo {
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				{ .pCombinedImageSampler = &sampler_info }
			};
		});

		const DU terrain_ds_updater = this->TerrainShaderDescriptorBuffer.createUpdater(ctx);
		for (const auto i : iota(0u, terrain_ds_update.size())) {
			terrain_ds_updater.update({
				.SetLayout = this->TerrainShaderLayout,
				.SetIndex = 0u,
				.Binding = i,
				.GetInfo = terrain_ds_update[i]
			});
		}
	}
}

inline VkDevice SimpleTerrain::getDevice() const noexcept {
	return this->UniformBuffer.second->get_deleter().Device;
}

inline VmaAllocator SimpleTerrain::getAllocator() const noexcept {
	return this->UniformBuffer.first->get_deleter().Allocator;
}

constexpr AccelStructManager::CompactionSizeQueryInfo SimpleTerrain::createCompactionQueryInfo(const VkQueryPool qp) const noexcept {
	return {
		.QueryPool = qp,
		.QueryIndex = 0u
	};
}

SimpleTerrain::AccelStructBuildTempMemory SimpleTerrain::buildTerrainAccelStruct(const VulkanContext& ctx,
	const VkCommandBuffer cmd, const VkQueryPool query) {
	using glm::mat3x4;
	static_assert(sizeof(VkTransformMatrixKHR) == sizeof(mat3x4));

	//////////////////////////////
	/// Terrain transform matrix
	//////////////////////////////
	VKO::BufferAllocation terrain_transform_mem = BufferManager::createTransientHostBuffer(
		{ this->getDevice(), this->getAllocator(), sizeof(VkTransformMatrixKHR) },
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		BufferManager::HostAccessPattern::Sequential);

	VKO::MappedAllocation transform_data = VKO::mapAllocation<mat3x4>(this->getAllocator(), terrain_transform_mem.first);
	*transform_data = mat3x4(glm::transpose(::TerrainUniformData.TerrainTransform.M));

	CHECK_VULKAN_ERROR(vmaFlushAllocation(this->getAllocator(), terrain_transform_mem.first, 0ull, VK_WHOLE_SIZE));
	transform_data.reset();

	///////////////
	/// Build GAS
	///////////////
	const auto compaction_query_info = this->createCompactionQueryInfo(query);
	const auto gas_entry = array {
		GeometryData::GeometryDataEntry {
			&this->AccelStructPlane,
			BufferManager::addressOf(this->getDevice(), terrain_transform_mem.second),
			0u
		}
	};
	auto [gas, scratch] = GeometryData::buildAccelStruct(ctx, cmd,
		VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR |
		VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR, gas_entry, &compaction_query_info);

	//////////////////////////////////
	/// Barrier for later compaction
	///////////////////////////////////
	PipelineBarrier<0u, 1u, 0u> barrier;
	barrier.addBufferBarrier({
		VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
		VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
		VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
		VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR
	}, gas.AccelStructMemory.second);
	barrier.record(cmd);

	using std::move;
	this->TerrainAccelStruct = move(gas);
	return make_tuple(
		move(terrain_transform_mem),
		move(scratch)
	);
}

AccelStructManager::AccelStruct SimpleTerrain::compactTerrainAccelStruct(const VkCommandBuffer cmd, const VkQueryPool query) const {
	const auto compaction_query_info = this->createCompactionQueryInfo(query);

	AccelStructManager::AccelStruct as = AccelStructManager::compactAccelStruct(this->TerrainAccelStruct.AccelStruct, {
		.Device = this->getDevice(),
		.Allocator = this->getAllocator(),
		.Command = cmd,
		
		.Type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
		.CompactionSizeQuery = &compaction_query_info
	});

	PipelineBarrier<0u, 1u, 0u> barrier;
	barrier.addBufferBarrier({
		VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
		VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
		SimpleWater::WaterCreateInfo::GASStage,
		SimpleWater::WaterCreateInfo::GASAccess
	}, as.AccelStructMemory.second);
	barrier.record(cmd);

	return as;
}

void SimpleTerrain::reshape(const ReshapeInfo& reshape_info) {
	const auto [ctx, extent] = reshape_info;

	this->OutputExtent = extent;
	this->OutputAttachment = FramebufferManager::createSimpleFramebuffer({
		.Device = this->getDevice(),
		.Allocator = this->getAllocator(),

		.ColourFormat = ::ColourFormat,
		.DepthFormat = ::DepthFormat,
		.Sample = ::TerrainSampleCount,

		.Extent = this->OutputExtent
	});

	const VkCommandBuffer cmd = this->TerrainReshapeCmd;
	CommandBufferManager::beginOneTimeSubmit(cmd);

	FramebufferManager::prepareFramebuffer(cmd, this->OutputAttachment, { VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL });

	CHECK_VULKAN_ERROR(vkEndCommandBuffer(cmd));
	CommandBufferManager::submit<1u>({ *ctx->Device, ctx->Queue.Render }, { cmd }, {{ }}, {{ }});

	if (this->WaterRenderer) {
		this->WaterRenderer->reshape(reshape_info);
	}
}

SimpleTerrain::DrawResult SimpleTerrain::draw(const DrawInfo& draw_info) {
	const auto& [ctx, camera, delta_time, frame_index, vp, draw_area, present_img, present_img_view] = draw_info;
	/*
	If we need to render water, we do not need to render and resolve the terrain to present image straight away,
	and pass the present image to water renderer, letting it finishes the rest.
	*/
	const bool draw_water = this->WaterRenderer.has_value();

	const VkCommandBuffer cmd = this->TerrainDrawCmd[frame_index];
	CommandBufferManager::beginOneTimeSubmit(cmd);

	/************************
	 * Subpass dependencies
	 ***********************/
	constexpr static FramebufferManager::PrepareFramebufferInfo prepare_info {
		.DepthLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
	};
	const FramebufferManager::SubpassOutputDependencyIssueInfo issue_info {
		.PrepareInfo = &prepare_info
	};
	FramebufferManager::issueSubpassOutputDependency(cmd, this->OutputAttachment, issue_info);

	SimpleWater::SceneDepthRecordInfo scene_depth_record_info;
	if (draw_water) {
		scene_depth_record_info = {
			.Stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			.Access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
			.Layout = prepare_info.DepthLayout
		};
		this->WaterRenderer->beginSceneDepthRecord(cmd, scene_depth_record_info);
	}

	/*********************
	 * Begin rendering
	 ********************/
	//BUG: I believe this is due to a bug in validation layer that reports undefined layout on the water scene depth image.
	CONTEXT_DISABLE_MESSAGE(msg_id, *ctx, 0x5D1FD459);
	FramebufferManager::beginInitialRendering(cmd, this->OutputAttachment, {
		.DependencyInfo = &issue_info,
		.ClearColour = glm::vec4(1.0f),
		.RenderArea = draw_area,
		.ResolveOutput = {
			.Depth = draw_water ? this->WaterRenderer->getSceneDepth() : VK_NULL_HANDLE
		},
		//sky renderer will always need to read from the current frame buffer
		.RequiredAfterRendering = {
			.Colour = true,
			.Depth = true
		}
	});
	CONTEXT_ENABLE_MESSAGE(*ctx, msg_id);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, this->Pipeline);
	vkCmdSetViewport(cmd, 0u, 1u, &vp);
	vkCmdSetScissor(cmd, 0u, 1u, &draw_area);

	/**************
	 * Descriptor
	 *************/
	const auto ds = array {
		camera->descriptorBufferBindingInfo(),
		VkDescriptorBufferBindingInfoEXT {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
			.address = BufferManager::addressOf(this->getDevice(), this->TerrainShaderDescriptorBuffer.buffer()),
			.usage = VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT
		}
	};
	array<uint32_t, ds.size()> ds_idx;
	std::iota(ds_idx.begin(), ds_idx.end(), 0u);
	const auto ds_offset = array {
		camera->descriptorBufferOffset(frame_index),
		this->TerrainShaderDescriptorBuffer.offset(0u)
	};

	vkCmdBindDescriptorBuffersEXT(cmd, static_cast<uint32_t>(ds.size()), ds.data());
	vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, this->PipelineLayout, 0u,
		static_cast<uint32_t>(ds.size()), ds_idx.data(), ds_offset.data());

	/*******************
	 * Buffer binding
	 ******************/
	const GeometryData::AttributeInfo& attr_info = this->Plane.attributeInfo();
	const auto [vertex_offset, index_offset, indirect_offset] = attr_info.Offset;
	const VkBuffer vbo = this->Plane.buffer();

	vkCmdBindVertexBuffers(cmd, 0u, 1u, &vbo, &vertex_offset);
	vkCmdBindIndexBuffer(cmd, vbo, index_offset, attr_info.Type.Index);

	/****************
	 * Draw terrain
	 ***************/
	vkCmdDrawIndexedIndirect(cmd, vbo, indirect_offset, 1u, 0u);
	vkCmdEndRendering(cmd);

	/**************
	 * Draw water
	 *************/
	FixedArray<VkCommandBuffer, 2u> draw_cmd;
	if (draw_water) {
		this->WaterRenderer->endSceneDepthRecord(cmd, scene_depth_record_info);

		const auto [water_cmd, water_wait_stage] = this->WaterRenderer->draw({
			.InheritedDrawInfo = &draw_info,
			.SceneGeometry = &this->AccelStructPlane,
			.InputFramebuffer = &this->OutputAttachment,
			.DepthLayout = prepare_info.DepthLayout
		});
		assert(water_wait_stage == VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

		draw_cmd.pushBack(water_cmd);
	}

	/***********
	 * Draw sky
	 ***********/
	{
		const auto [sky_cmd, sky_wait_stage] = this->SkyRenderer.draw({
			.InheritedDrawInfo = &draw_info,
			.InputFramebuffer = &this->OutputAttachment,
			.DepthLayout = prepare_info.DepthLayout
		});
		assert(sky_wait_stage == VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

		draw_cmd.pushBack(sky_cmd);
	}

	/****************************
	 * Prepare for presentation
	 ***************************/
	vkCmdExecuteCommands(cmd, static_cast<uint32_t>(draw_cmd.size()), draw_cmd.data());
	FramebufferManager::transitionAttachmentToPresent(cmd, present_img);

	CHECK_VULKAN_ERROR(vkEndCommandBuffer(cmd));
	return {
		.DrawCommand = cmd,
		.WaitStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
	};
}