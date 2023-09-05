#include "PlaneGeometry.hpp"

#include "../Common/ErrorHandler.hpp"
#include "../Common/File.hpp"

#include "../Engine/Abstraction/BufferManager.hpp"
#include "../Engine/Abstraction/CommandBufferManager.hpp"
#include "../Engine/Abstraction/PipelineBarrier.hpp"
#include "../Engine/Abstraction/ShaderModuleManager.hpp"
#include "../Engine/EngineSetting.hpp"
#include "../Engine/IndirectCommand.hpp"

#include <shaderc/shaderc.h>

#include <glm/vec3.hpp>

#include <array>
#include <string_view>
#include <utility>
#include <algorithm>
#include <ranges>

#include <type_traits>
#include <stdexcept>

using glm::uvec2, glm::vec2, glm::dvec2, glm::u16vec2,
	glm::vec3;

using std::array, std::string_view;
using std::ranges::transform, std::views::iota;
using std::ostream, std::endl;

using namespace LearnVulkan;
namespace VKO = VulkanObject;

#define PLANE_COMMAND_BUFFER_INDEX(TYPE) static_cast<std::underlying_type_t<::PlaneCommandBufferIndex>>(::PlaneCommandBufferIndex::TYPE)

namespace {

	constexpr auto GeneratorLocalSize = uvec2(16u, 16u);
	constexpr VkFormat VertexFormat = VK_FORMAT_R32G32B32_SFLOAT;

	/**
	 * @brief Used to index individual command buffer with an array of allocated command buffers.
	*/
	enum class PlaneCommandBufferIndex : size_t {
		Generate,
		Displace,
		Count
	};

	//////////////////////////
	/// Shader data structure
	//////////////////////////
	struct VertexAttribute {
		
		vec3 Pos;
		u16vec2 UV;

	};
	struct IndexAttribute {

		uint32_t I[6];

	};
	struct PlaneInputParameter {

		//This order of member perfectly satisfies std430 alignment requirement,
		//and no padding is required, Hooray!!!
		dvec2 Dim, TotPln;
		uvec2 Sub, VerDim;
		uint32_t IC;

	};
	struct PlaneAttribute {

		struct {

			uint32_t Vertex, Index;

		} Size;/**< The size of vertex attribute in plane geometry buffer, in byte. */
		struct {

			uint32_t Primitive, Vertex;

		} Count;/**< The number of each attribute. */
		uvec2 ThreadCount;

	};
	struct GenerateInfo {

		VkDeviceAddress V, I, C;

	};
	struct DisplaceInfo {
		
		VkDeviceAddress V;
		float Alt;

	};

	struct PlanePrivateData {

		uvec2 ThreadCount;

	};

	///////////////////////
	/// Shader information
	///////////////////////
	constexpr string_view PlaneGeneratorCS = "/PlaneGenerator.comp", PlaneDisplacerCS = "/PlaneDisplacer.comp";
	constexpr auto PlaneShaderFilenameRaw = File::toAbsolutePath<EngineSetting::ShaderRoot, PlaneGeneratorCS, PlaneDisplacerCS>();
	constexpr auto PlaneShaderFilename = File::batchRawStringToView(PlaneShaderFilenameRaw);

	////////////////////////
	/// Setup
	///////////////////////
	inline ShaderModuleManager::ShaderOutputGenerator compilePlaneShader(const VkDevice device, ostream& msg) {
		msg << "Compiling plane geometry generation shader" << endl;

		constexpr static auto compute_shader = []() constexpr {
			array<shaderc_shader_kind, PlaneShaderFilename.size()> compute_shader;
			std::ranges::fill(compute_shader, shaderc_compute_shader);
			return compute_shader;
		}();
		const ShaderModuleManager::ShaderBatchCompilationInfo plane_info {
			.Device = device,
			.ShaderFilename = PlaneShaderFilename.data(),
			.ShaderKind = compute_shader.data()
		};
		return ShaderModuleManager::batchShaderCompilation<compute_shader.size()>(&plane_info, &msg);
	}

	inline VKO::DescriptorSetLayout createPlanePropertyDescriptorSetLayout(const VkDevice device) {
		constexpr static VkDescriptorSetLayoutBinding property {
			.binding = 0u,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1u,
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
		};
		constexpr static VkDescriptorSetLayoutCreateInfo plane_ds {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
			.bindingCount = 1u,
			.pBindings = &property
		};
		return VKO::createDescriptorSetLayout(device, plane_ds);
	}

	inline VKO::DescriptorSetLayout createPlaneDisplacementMapDescriptorSetLayout(const VkDevice device) {
		constexpr static VkDescriptorSetLayoutBinding property {
			.binding = 0u,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 1u,
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
		};
		constexpr static VkDescriptorSetLayoutCreateInfo plane_ds {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT | VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
			.bindingCount = 1u,
			.pBindings = &property
		};
		return VKO::createDescriptorSetLayout(device, plane_ds);
	}

	template<class TPushConstant, size_t DSCount>
	inline VKO::PipelineLayout createPlanePipelineLayout(const VkDevice device,
		const array<VkDescriptorSetLayout, DSCount> ds_layout) {
		constexpr static VkPushConstantRange arg_input {
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			.offset = 0u,
			.size = static_cast<uint32_t>(sizeof(TPushConstant))
		};
		return VKO::createPipelineLayout(device, {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.setLayoutCount = static_cast<uint32_t>(ds_layout.size()),
			.pSetLayouts = ds_layout.data(),
			.pushConstantRangeCount = 1u,
			.pPushConstantRanges = &arg_input
		});
	}

	//Returns an array of pipelines, depends on how many compute shaders we have.
	template<size_t LayoutCount>
	auto createPlanePipeline(const VkDevice device, const array<VkPipelineLayout, LayoutCount> layout, ostream& msg) {
		const auto plane_shader_gen = compilePlaneShader(device, msg);

		using Constant_t = array<uint32_t, 2u>;
		constexpr static Constant_t constant = {
			//FIXME: It's better to determine local size from physical device attribute, rather than fixed hand typed.
			//local size
			::GeneratorLocalSize.x, ::GeneratorLocalSize.y
		};
		array<VkSpecializationMapEntry, constant.size()> map_entry;
		transform(iota(size_t { 0 }, map_entry.size()), map_entry.begin(), [](const auto i) constexpr noexcept {
			constexpr static uint32_t entry_size = static_cast<uint32_t>(sizeof(Constant_t::value_type));
			const uint32_t index = static_cast<uint32_t>(i);
			
			return VkSpecializationMapEntry {
				.constantID = index,
				.offset = index * entry_size,
				.size = entry_size
			};
		});
		//we will be using the same specialisation across all shaders
		const VkSpecializationInfo spec_info {
			.mapEntryCount = static_cast<uint32_t>(constant.size()),
			.pMapEntries = map_entry.data(),
			.dataSize = static_cast<uint32_t>(constant.size() * sizeof(Constant_t::value_type)),
			.pData = constant.data()
		};

		array<VKO::Pipeline, PlaneShaderFilename.size()> pipeline;
		transform(layout, pipeline.begin(), [device, &gen = plane_shader_gen, &spec_info](const VkPipelineLayout layout) {
			gen.resume();
			const auto [sm_info, stage] = gen.promise();
			
			return VKO::createComputePipeline(device, VK_NULL_HANDLE, {
				.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
				.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
#ifndef NDEBUG
				| VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT
#endif
				,
				.stage = {
					.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
					.pNext = sm_info,
					.stage = stage,
					.pName = "main",
					.pSpecializationInfo = &spec_info
				},
				.layout = layout
			});	
		});
		return pipeline;
	}

	inline VKO::CommandBufferArray createPlaneCommandBuffer(const VkDevice device, const VkCommandPool cmd_pool) {
		return VKO::allocateCommandBuffers(device, {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = cmd_pool,
			.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY,
			.commandBufferCount = PLANE_COMMAND_BUFFER_INDEX(Count)
		});
	}

	inline VKO::BufferAllocation createPlaneGeometryDataBuffer(const VkDevice device, const VmaAllocator allocator, const size_t size,
		const bool require_as) {
		VkBufferUsageFlags flag = VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
			VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		if (require_as) {
			flag |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
		}
		return BufferManager::createDeviceBuffer({ device, allocator, size }, flag);
	}

	constexpr ::PlaneAttribute calcPlaneAttribute(const PlaneGeometry::Property& prop, ::PlaneInputParameter& input_param) noexcept {
		const auto& [dim, subdivision, require_build_accel_struct] = prop;

		const uvec2 vertex_dimension = subdivision + 1u;
		const uint32_t vertex_data_count = vertex_dimension.x * vertex_dimension.y,
			index_data_count = subdivision.x * subdivision.y,
			
			vertex_data_size = vertex_data_count * sizeof(::VertexAttribute),
			index_data_size = index_data_count * sizeof(::IndexAttribute);

		input_param = {
			.Dim = dim,
			.TotPln = dvec2(subdivision),
			.Sub = subdivision,
			.VerDim = vertex_dimension,
			//6 indices per subdivision
			.IC = index_data_count * 6u
		};
		return {
			.Size = {
				.Vertex = vertex_data_size,
				.Index = index_data_size
			},
			.Count = {
				//Each subdivision is a quad, which contains 2 triangles.
				//Our primitive unit is triangle, so the primitive count is twice the number of subdivision.
				.Primitive = index_data_count * 2u,
				.Vertex = vertex_data_count
			},
			.ThreadCount = vertex_dimension
		};
	}

}

VkCommandBuffer PlaneGeometry::prepareGeometryData(const VulkanContext& ctx, const Property& prop, GeometryData& geo) const {
	VKO::BufferAllocation& input_param_staging = geo.Temporary.InputParameterStaging;
	{
		/*****************************
		 * Populate input parameters
		 ****************************/
		input_param_staging = BufferManager::createStagingBuffer({ ctx.Device, ctx.Allocator,
			sizeof(::PlaneInputParameter) }, BufferManager::HostAccessPattern::Sequential);

		void* geo_input_mem;
		CHECK_VULKAN_ERROR(vmaMapMemory(ctx.Allocator, input_param_staging.first, &geo_input_mem));
		::PlaneInputParameter* const plane_input_param = new(geo_input_mem) ::PlaneInputParameter;

		const ::PlaneAttribute plane_attr = ::calcPlaneAttribute(prop, *plane_input_param);

		CHECK_VULKAN_ERROR(vmaFlushAllocation(ctx.Allocator, input_param_staging.first, 0ull, VK_WHOLE_SIZE));
		vmaUnmapMemory(ctx.Allocator, input_param_staging.first);
		(void)geo_input_mem;

		/****************************
		 * Initialise geometry data
		 ***************************/
		const auto [vertex_size, index_size] = plane_attr.Size;
		const auto [primitive_count, vertex_count] = plane_attr.Count;
		const VkDeviceSize vi_size = vertex_size + index_size;

		geo.Type = GeometryData::GeometryType::Plane;
		geo.Attribute = {
			.Offset = {
				.Vertex = 0ull,
				.Index = vertex_size,
				.Indirect = vi_size
			},
			.Count = {
				.Primitive = primitive_count,
				.Vertex = vertex_count
			},
			.Stride = static_cast<VkDeviceSize>(sizeof(::VertexAttribute)),
			.Type = {
				.Vertex = ::VertexFormat,
				.Index = VK_INDEX_TYPE_UINT32
			}
		};
		geo.PrivateData.emplace<::PlanePrivateData>(::PlanePrivateData {
			.ThreadCount = plane_attr.ThreadCount
		});

		geo.Command = createPlaneCommandBuffer(ctx.Device, ctx.CommandPool.General);

		//TODO: As an optimisation, we can check if the input geometry data was previously used as the same geometry type,
		//(in this case, plane geometry). If so, we don't need to reallocate memory for input parameters and its descriptor buffer,
		//since the size is constant.
		//If this approach is taken, remember to check if the temporary memories are released.
		geo.Memory = {
			.Geometry = createPlaneGeometryDataBuffer(ctx.Device, ctx.Allocator,
				vi_size + sizeof(IndirectCommand::VkDrawIndexedIndirectCommand), prop.RequireAccelStructInput),
			.InputParameter = BufferManager::createDeviceBuffer({ ctx.Device, ctx.Allocator, sizeof(::PlaneInputParameter) },
				VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
		};
	}
	const VkCommandBuffer copy_cmd = geo.Command[PLANE_COMMAND_BUFFER_INDEX(Generate)];
	/***************************************
	 * Prepare plane input parameter buffer
	 ***************************************/
	{
		CHECK_VULKAN_ERROR(vkResetCommandBuffer(copy_cmd, { }));
		CommandBufferManager::beginOneTimeSubmitSecondary(copy_cmd);

		/*******************
		 * Copy to device
		 ******************/
		BufferManager::recordCopyBuffer(input_param_staging.second, geo.Memory.InputParameter.second, copy_cmd, sizeof(::PlaneInputParameter));
		
		PipelineBarrier<0u, 1u, 0u> barrier;
		barrier.addBufferBarrier({
			VK_PIPELINE_STAGE_2_COPY_BIT,
			VK_ACCESS_2_TRANSFER_WRITE_BIT,
			VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
			VK_ACCESS_2_SHADER_STORAGE_READ_BIT
		}, geo.Memory.InputParameter.second);
		barrier.record(copy_cmd);
	}
	/*****************************
	 * Prepare descriptor buffer
	 ****************************/
	{
		const auto plane_ds_layout = array { *this->DescriptorSet.PlaneProperty };
		geo.InputParameterDescriptorBuffer = DescriptorBufferManager(ctx, plane_ds_layout,
			VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT);

		const VkDescriptorAddressInfoEXT storage_addr {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT,
			.address = BufferManager::addressOf(ctx.Device, geo.Memory.InputParameter.second),
			.range = sizeof(::PlaneInputParameter)
		};

		const DescriptorBufferManager::DescriptorUpdater plane_updater = geo.InputParameterDescriptorBuffer.createUpdater(ctx);
		plane_updater.update({
			.SetLayout = this->DescriptorSet.PlaneProperty,
			.SetIndex = 0u,
			.GetInfo = {
				VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				{ .pStorageBuffer = &storage_addr }
			}
		});
	}

	return copy_cmd;
}

inline void PlaneGeometry::bindDescriptorBuffer(const VkDevice device, const VkCommandBuffer cmd,
	const VkPipelineLayout layout, const GeometryData& geo) noexcept {
	const VkDescriptorBufferBindingInfoEXT prop_binding {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
		.address = BufferManager::addressOf(device, geo.InputParameterDescriptorBuffer.buffer()),
		.usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT
	};
	constexpr static uint32_t buf_idx = 0u;
	vkCmdBindDescriptorBuffersEXT(cmd, 1u, &prop_binding);
	vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout,
		0u, 1u, &buf_idx, geo.InputParameterDescriptorBuffer.offset().data());
}

inline void PlaneGeometry::dispatch(const VkCommandBuffer cmd, const GeometryData& geo, const uint32_t workgroup_count_z) {
	const uvec2 workgroup_count = (std::any_cast<const ::PlanePrivateData&>(geo.PrivateData).ThreadCount
		+ ::GeneratorLocalSize - 1u) / ::GeneratorLocalSize;
	vkCmdDispatch(cmd, workgroup_count.x, workgroup_count.y, workgroup_count_z);
}

PlaneGeometry::PlaneGeometry(const VulkanContext& ctx, ostream& msg) :
	DescriptorSet {
		.PlaneProperty = createPlanePropertyDescriptorSetLayout(ctx.Device),
		.DisplacementMap = createPlaneDisplacementMapDescriptorSetLayout(ctx.Device)
	},
	PipelineLayout {
		.Generator = createPlanePipelineLayout<::GenerateInfo>(ctx.Device, array { *this->DescriptorSet.PlaneProperty }),
		.Displacer = createPlanePipelineLayout<::DisplaceInfo>(ctx.Device, array {
			*this->DescriptorSet.PlaneProperty,
			*this->DescriptorSet.DisplacementMap
		})
	} {
	const auto& [gen_layout, disp_layout] = this->PipelineLayout;
	auto [gen_pipeline, disp_pipeline] = createPlanePipeline(ctx.Device, array { *gen_layout, *disp_layout }, msg);

	using std::move;
	this->Pipeline = {
		.Generator = move(gen_pipeline),
		.Displacer = move(disp_pipeline)
	};
}

PlaneGeometry::VertexInput PlaneGeometry::vertexInput(const VertexInputCustomisation& customisation) noexcept {
	const auto& [binding, location] = customisation;
	const auto& [loc_pos, loc_uv] = location;

	decltype(VertexInput::Attribute) attribute;
	if (loc_pos) {
		attribute.pushBack({
			.location = *loc_pos,
			.binding = binding,
			.format = ::VertexFormat,
			.offset = offsetof(::VertexAttribute, Pos)
		});
	}
	if (loc_uv) {
		attribute.pushBack({
			.location = *loc_uv,
			.binding = binding,
			.format = VK_FORMAT_R16G16_UNORM,
			.offset = offsetof(::VertexAttribute, UV)
		});
	}
	return {
		.Binding = {
			.binding = binding,
			.stride = sizeof(::VertexAttribute),
			.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
		},
		.Attribute = attribute
	};
}

VkCommandBuffer PlaneGeometry::generate(const VulkanContext& ctx, const Property& prop, GeometryData& geo) const {
	const VkCommandBuffer cmd = this->prepareGeometryData(ctx, prop, geo);
	const VkDevice device = ctx.Device;
	
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, this->Pipeline.Generator);
	PlaneGeometry::bindDescriptorBuffer(device, cmd, this->PipelineLayout.Generator, geo);

	///////////////////////////
	/// Prepare input argument
	///////////////////////////
	const VkDeviceAddress output = BufferManager::addressOf(device, geo.Memory.Geometry.second);
	const auto [vertex_offset, index_offset, cmd_offset] = geo.Attribute.Offset;
	const ::GenerateInfo gen_info {
		output + vertex_offset,
		output + index_offset,
		output + cmd_offset
	};
	vkCmdPushConstants(cmd, this->PipelineLayout.Generator, VK_SHADER_STAGE_COMPUTE_BIT, 0u,
		static_cast<uint32_t>(sizeof(gen_info)), &gen_info);

	//////////////
	/// Dispatch
	//////////////
	//Z-axis has dimension of 2, one for vertex and the other for index.
	PlaneGeometry::dispatch(cmd, geo, 2u);

	CHECK_VULKAN_ERROR(vkEndCommandBuffer(cmd));
	return cmd;
}

VkCommandBuffer PlaneGeometry::displace(const VulkanContext& ctx, const Displacement& disp, GeometryData& geo) const {
	if (geo.Type != GeometryData::GeometryType::Plane) {
		throw std::runtime_error("Cannot perform displacement on non-plane geometry.");
	}
	const VkCommandBuffer cmd = geo.Command[PLANE_COMMAND_BUFFER_INDEX(Displace)];
	const VkDevice device = ctx.Device;

	CHECK_VULKAN_ERROR(vkResetCommandBuffer(cmd, { }));
	CommandBufferManager::beginOneTimeSubmitSecondary(cmd);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, this->Pipeline.Displacer);
	PlaneGeometry::bindDescriptorBuffer(device, cmd, this->PipelineLayout.Displacer, geo);

	//////////////////
	/// Shader input
	//////////////////
	const VkDeviceAddress addr = BufferManager::addressOf(device, geo.Memory.Geometry.second);
	const ::DisplaceInfo disp_info {
		addr + geo.Attribute.Offset.Vertex,
		disp.Altitude
	};
	vkCmdPushConstants(cmd, this->PipelineLayout.Displacer, VK_SHADER_STAGE_COMPUTE_BIT, 0u,
		static_cast<uint32_t>(sizeof(disp_info)), &disp_info);

	const VkWriteDescriptorSet disp_map {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstBinding = 0u,
		.dstArrayElement = 0u,
		.descriptorCount = 1u,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.pImageInfo = &disp.DisplacementMap
	};
	vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, this->PipelineLayout.Displacer, 1u, 1u, &disp_map);

	/////////////
	/// Dispatch
	/////////////
	PlaneGeometry::dispatch(cmd, geo, 1u);

	CHECK_VULKAN_ERROR(vkEndCommandBuffer(cmd));
	return cmd;
}