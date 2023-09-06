#pragma once

#include "../../Common/VulkanObject.hpp"

#include <array>
#include <span>
#include <cstdint>

namespace LearnVulkan {

	/**
	 * @brief A utility to quickly build, update and compact acceleration structure for ray-tracing.
	*/
	namespace AccelStructManager {

		struct CompactionSizeQueryInfo {

			VkQueryPool QueryPool;
			uint32_t QueryIndex;

		};

		/**
		 * @brief Information to build an acceleration structure.
		*/
		struct AccelStructBuildInfo {

			VkDevice Device;
			VmaAllocator Allocator;
			VkCommandBuffer Command;

			VkAccelerationStructureTypeKHR Type;
			VkBuildAccelerationStructureFlagsKHR Flag;

			//Optional input query pool to query the compaction size of acceleration structure.
			const CompactionSizeQueryInfo* CompactionSizeQuery = nullptr;

		};

		/**
		 * @brief Information to compact an acceleration structure.
		 * This is just an alias of the build info, except:
		 * - Build flag is ignored.
		 * - Compaction size query must not be null.
		*/
		using AccelStructCompactInfo = AccelStructBuildInfo;

		/**
		 * @brief Contains the built acceleration structure.
		*/
		struct AccelStruct {

			VulkanObject::BufferAllocation AccelStructMemory;
			VulkanObject::AccelerationStructureKHR AccelStruct;

		};

		/**
		 * @brief Store the result after issuing an acceleration structure build command.
		*/
		struct AccelStructBuildResult {
			
			AccelStruct AccelerationStructure;
			//This temporary memory must be retained until acceleration structure has been built.
			VulkanObject::BufferAllocation ScratchMemory;

		};

		namespace _Detail {
		
			//See the public interface version of this function.
			AccelStructBuildResult buildAccelStruct(
				const AccelStructBuildInfo&,
				std::span<const VkAccelerationStructureGeometryKHR>,
				std::span<const VkAccelerationStructureBuildRangeInfoKHR>,
				std::span<uint32_t>
			);
		
		}
	
		/**
		 * FIXME: To make build operation more efficient, consider grouping multiple build operations in one go,
		 * this can save API round-trip and memory consumption of scratch memory.
		 * Here we are only building one acceleration structure at a time for simplicity.
		 * 
		 * @brief Initiate a device command to build an acceleration structure for a given geometry.
		 * No pipeline barrier is provided.
		 * @tparam GeometryCount The number of geometry info.
		 * @param build_info The acceleration structure build info.
		 * @param geometry An array of geometry.
		 * @param range An array of geometry range.
		 * @return The acceleration structure build result.
		 * @see AccelStructBuildInfo, AccelStructBuildResult
		*/
		template<size_t GeometryCount>
		inline AccelStructBuildResult buildAccelStruct(
			const AccelStructBuildInfo& build_info,
			const std::array<VkAccelerationStructureGeometryKHR, GeometryCount>& geometry,
			const std::array<VkAccelerationStructureBuildRangeInfoKHR, GeometryCount>& range
		) {
			using std::array;
			array<uint32_t, GeometryCount> max_primitive_count;

			return _Detail::buildAccelStruct(build_info, geometry, range, max_primitive_count);
		}

		/**
		 * @brief Perform a compaction for an acceleration structure.
		 * @param as The input acceleration structure.
		 * @param compact_info The information to compact an acceleration structure.
		 * @return The newly created compacted acceleration structure.
		*/
		AccelStruct compactAccelStruct(VkAccelerationStructureKHR, const AccelStructCompactInfo&);

		/**
		 * @brief Retrieve the address of an acceleration structure.
		 * @param device The device.
		 * @param as The acceleration structure whose device address should be obtained.
		 * @return The device address of the given acceleration structure.
		*/
		VkDeviceAddress addressOf(VkDevice, VkAccelerationStructureKHR) noexcept;

	}

}