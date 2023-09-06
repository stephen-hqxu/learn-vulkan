#include "AccelStructManager.hpp"
#include "PipelineBarrier.hpp"

#include "../../Common/ErrorHandler.hpp"

#include "BufferManager.hpp"

#include <utility>
#include <algorithm>

using std::span;
using std::ranges::transform;

using namespace LearnVulkan;
namespace VKO = VulkanObject;

#define EXPAND_COMPACTION_QUERY const auto [query_pool, query_idx] = *compaction_query

AccelStructManager::AccelStructBuildResult AccelStructManager::_Detail::buildAccelStruct(
	const AccelStructBuildInfo& build_info,
	const span<const VkAccelerationStructureGeometryKHR> geometry,
	const span<const VkAccelerationStructureBuildRangeInfoKHR> range,
	const span<uint32_t> max_primitive_count
) {
	const auto [device, allocator, cmd, type, flag, compaction_query] = build_info;

	/****************************
	 * Query memory requirement
	 ****************************/
	VkAccelerationStructureBuildGeometryInfoKHR vk_build_info {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
		.type = type,
		.flags = flag,
		.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
		.geometryCount = static_cast<uint32_t>(geometry.size()),
		.pGeometries = geometry.data()
	};
	transform(range, max_primitive_count.begin(), [](const auto& range) { return range.primitiveCount; });

	VkAccelerationStructureBuildSizesInfoKHR size_info {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
	};
	vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&vk_build_info, max_primitive_count.data(), &size_info);
	
	/********************************
	 * Create acceleration structure
	 ********************************/
	VKO::BufferAllocation as_memory = BufferManager::createDeviceBuffer({ device, allocator, size_info.accelerationStructureSize },
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR),
		scratch_memory = BufferManager::createDeviceBuffer({ device, allocator, size_info.buildScratchSize },
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
	const VkAccelerationStructureCreateInfoKHR as_create_info {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
		.buffer = as_memory.second,
		.offset = 0ull,
		.size = size_info.accelerationStructureSize,
		.type = type
	};
	VKO::AccelerationStructureKHR as = VKO::createAccelerationStructureKHR(device, as_create_info);

	/******************************
	 * Build acceleration structure
	 ******************************/
	vk_build_info.dstAccelerationStructure = as;
	vk_build_info.scratchData = { .deviceAddress = BufferManager::addressOf(device, scratch_memory.second) };

	const VkAccelerationStructureBuildRangeInfoKHR* const range_ptr = range.data();
	vkCmdBuildAccelerationStructuresKHR(cmd, 1u, &vk_build_info, &range_ptr);

	if (compaction_query) {
		EXPAND_COMPACTION_QUERY;
		
		PipelineBarrier<0u, 1u, 0u> barrier;
		barrier.addBufferBarrier({
			VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
			VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR
		}, as_memory.second);
		barrier.record(cmd);

		vkCmdWriteAccelerationStructuresPropertiesKHR(cmd, 1u, &vk_build_info.dstAccelerationStructure,
			VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, query_pool, query_idx);
	}

	using std::move;
	return {
		{
			move(as_memory),
			move(as)
		},
		move(scratch_memory)
	};
}

AccelStructManager::AccelStruct AccelStructManager::compactAccelStruct(
	const VkAccelerationStructureKHR as, const AccelStructCompactInfo& compact_info) {
	const auto [device, allocator, cmd, type, flag, compaction_query] = compact_info;
	EXPAND_COMPACTION_QUERY;

	uint32_t size;
	//Typically in practice, we can query available instead of waiting.
	//If not available, we can move on (i.e., keep rendering using the old acceleration structure), and come back later.
	CHECK_VULKAN_ERROR(vkGetQueryPoolResults(device, query_pool, query_idx, 1u,
		sizeof(size), &size, sizeof(size), VK_QUERY_RESULT_WAIT_BIT));

	VKO::BufferAllocation compacted_buf = BufferManager::createDeviceBuffer({ device, allocator, size },
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR);
	VKO::AccelerationStructureKHR compacted_as = VKO::createAccelerationStructureKHR(device, {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
		.buffer = compacted_buf.second,
		.offset = 0ull,
		.size = size,
		.type = type
	});

	const VkCopyAccelerationStructureInfoKHR copy_info {
		.sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR,
		.src = as,
		.dst = compacted_as,
		.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR
	};
	vkCmdCopyAccelerationStructureKHR(cmd, &copy_info);

	using std::move;
	return {
		move(compacted_buf),
		move(compacted_as)
	};
}

VkDeviceAddress AccelStructManager::addressOf(const VkDevice device, const VkAccelerationStructureKHR as) noexcept {
	const VkAccelerationStructureDeviceAddressInfoKHR addr_info {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
		.accelerationStructure = as
	};
	return vkGetAccelerationStructureDeviceAddressKHR(device, &addr_info);
}