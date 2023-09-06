#include "GeometryData.hpp"

#include "../Engine/Abstraction/BufferManager.hpp"
#include "../Engine/Abstraction/PipelineBarrier.hpp"

#include <stdexcept>

using namespace LearnVulkan;
namespace VKO = VulkanObject;

GeometryData::GeometryData() noexcept : Type(GeometryType::Uninitialised), Attribute { } {

}

VkBuffer GeometryData::buffer() const noexcept {
	return this->Memory.Geometry.second;
}

void GeometryData::releaseTemporary() noexcept {
	this->Temporary = { };
}

const GeometryData::AttributeInfo& GeometryData::attributeInfo() const noexcept {
	return this->Attribute;
}

void GeometryData::accelerationStructureGeometry(VkAccelerationStructureGeometryKHR& as_geo, const VkDeviceAddress transform_addr) const noexcept {
	using BufferManager::addressOf;

	const VkDeviceAddress geometry_addr = addressOf(this->Memory.Geometry.second->get_deleter().Device, this->Memory.Geometry.second);
	const auto [vertex_offset, index_offset, indirect_offset] = this->Attribute.Offset;
	const auto [vertex_type, index_type] = this->Attribute.Type;
	as_geo = {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
		.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
		.geometry = {
			.triangles = {
				.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
				.vertexFormat = vertex_type,
				.vertexData = { .deviceAddress = geometry_addr + vertex_offset },
				.vertexStride = this->Attribute.Stride,
				.maxVertex = this->Attribute.Count.Vertex,
				.indexType = index_type,
				.indexData = { .deviceAddress = geometry_addr + index_offset },
				.transformData = { .deviceAddress = transform_addr }
			}
		},
		.flags = VK_GEOMETRY_OPAQUE_BIT_KHR
	};
}

void GeometryData::accelerationStructureRange(VkAccelerationStructureBuildRangeInfoKHR& as_range, const uint32_t transform_offset) const noexcept {
	as_range = {
		.primitiveCount = this->Attribute.Count.Primitive,
		.primitiveOffset = 0u,
		.firstVertex = 0u,
		.transformOffset = transform_offset
	};
}

void GeometryData::barrier(const VkCommandBuffer cmd, const BarrierTarget src_target, const BarrierTarget dst_target) const {
	constexpr static auto get_stage_access = [](const BarrierTarget target) -> std::pair<VkPipelineStageFlags2, VkAccessFlags2> {
		using enum BarrierTarget;
		switch (target) {
		case Generation: return { VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT };
		case Displacement: return { VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
			VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT };
		case Rendering: return {
			VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,
			VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_2_INDEX_READ_BIT | VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT
		};
		case AccelStructBuild: return {
			VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR
		};
		default:
			throw std::runtime_error("Unable to determine the barrier target.");
		}
	};
	const auto [src_stage, src_access] = get_stage_access(src_target);
	const auto [dst_stage, dst_access] = get_stage_access(dst_target);
	
	PipelineBarrier<0u, 1u, 0u> barrier;
	barrier.addBufferBarrier({
		src_stage,
		src_access,
		dst_stage,
		dst_access
	}, this->Memory.Geometry.second);
	barrier.record(cmd);
}