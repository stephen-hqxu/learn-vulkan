#pragma once

#include "../Engine/Abstraction/AccelStructManager.hpp"
#include "../Engine/Abstraction/DescriptorBufferManager.hpp"
#include "../Engine/VulkanContext.hpp"

#include "../Common/VulkanObject.hpp"

#include <array>
#include <any>
#include <cstdint>

namespace LearnVulkan {

	/**
	 * @brief Contains data of the generated geometry.
	*/
	class GeometryData {
	public:

		friend class PlaneGeometry;

		/**
		 * @brief The type of geometry that the geometry data class is initialised and valid for.
		*/
		enum class GeometryType : uint8_t {
			Plane = 0x00u,
			Uninitialised = 0xFFu
		};

		/**
		 * @brief Specify the target where pipeline barrier should be issued.
		*/
		enum class BarrierTarget : uint8_t {
			Generation = 0x00u,/**< Geometry generation. */
			Displacement = 0x01u,/**< Geometry displacement. */
			Rendering = 0x10u,/**< Rendering operation that uses geometry data. */
			AccelStructBuild = 0x20u/**< Acceleration structure build operation uses geometry data. */
		};

		/**
		 * @brief Containing information regarding attribute.
		*/
		struct AttributeInfo {

			struct {

				VkDeviceSize Vertex, Index, Indirect;

			} Offset;/**< Offset information into different fields of geometry buffer, in byte. */
			struct {
			
				uint32_t Primitive, Vertex;
			
			} Count;

			VkDeviceSize Stride;/**< The number of byte between each attribute. */
			struct {

				VkFormat Vertex;
				VkIndexType Index;

			} Type;/**< Format of vertex and index. */

		};

		/**
		 * @brief A single entry of geometry data when building acceleration structure.
		*/
		struct GeometryDataEntry {

			const GeometryData* Geometry;

			VkDeviceAddress TransformMatrix;
			uint32_t TransformMatrixMemoryOffset;

		};

	private:

		GeometryType Type;
		AttributeInfo Attribute;
		std::any PrivateData;/**< Opaque data used by geometry generator. */

		VulkanObject::CommandBufferArray Command;/**< The number of allocated command buffer depends on type of generator and geometry. */

		struct {

			VulkanObject::BufferAllocation Geometry,/**< Vertex, index and indirect draw command. */
				InputParameter;/**< Opaque generation parameters. */

		} Memory;

		DescriptorBufferManager InputParameterDescriptorBuffer;

		struct {

			VulkanObject::BufferAllocation InputParameterStaging;

		} Temporary;

	public:

		/**
		 * @brief Initialise a geometry data, uninitialised with undefined data.
		*/
		GeometryData() noexcept;

		GeometryData(const GeometryData&) = delete;

		GeometryData(GeometryData&&) noexcept = default;

		GeometryData& operator=(const GeometryData&) = delete;

		GeometryData& operator=(GeometryData&&) noexcept = default;

		~GeometryData() = default;

		/**
		 * @brief Get the buffer containing geometry data.
		 * @return The geometry buffer.
		*/
		VkBuffer buffer() const noexcept;

		/**
		 * @brief Release all temporary memory back to the system.
		 * This function must not be called while any temporary memory is still being used.
		*/
		void releaseTemporary() noexcept;

		/**
		 * @brief Get attribute information about the geometry data buffer.
		 * @return Attribute information.
		*/
		const AttributeInfo& attributeInfo() const noexcept;

		/**
		 * @brief Get the geometry data for acceleration structure.
		 * The geometry is assumed to consist of triangles only, and is opaque.
		 * @param as_geo The output geometry for acceleration structure build.
		 * @param transform_addr The address to the transform matrix.
		*/
		void accelerationStructureGeometry(VkAccelerationStructureGeometryKHR&, VkDeviceAddress) const noexcept;

		/**
		 * @brief Get the acceleration structure range info for the current geometry.
		 * @param as_range The output range.
		 * @param transform_offset The offset into the transform matrix address.
		*/
		void accelerationStructureRange(VkAccelerationStructureBuildRangeInfoKHR&, uint32_t) const noexcept;

		/**
		 * @brief Record an acceleration structure build command from an array of geometry data.
		 * The acceleration structure will be a BLAS.
		 * @tparam GeometryCount The number of geometry data.
		 * @param ctx The vulkan context.
		 * @param cmd The command buffer.
		 * @param flag Specify any acceleration structure build flag.
		 * @param geometry An array of geometry data.
		 * @param query_info An optional pointer to query compaction size of acceleration structure.
		 * @return The acceleration structure build result.
		*/
		template<size_t GeometryCount>
		static AccelStructManager::AccelStructBuildResult buildAccelStruct(
			const VulkanContext& ctx,
			const VkCommandBuffer cmd,
			const VkBuildAccelerationStructureFlagsKHR flag,
			const std::array<GeometryDataEntry, GeometryCount>& geometry,
			const AccelStructManager::CompactionSizeQueryInfo* query_info = nullptr
		) {
			using std::array;
			array<VkAccelerationStructureGeometryKHR, GeometryCount> as_geometry;
			array<VkAccelerationStructureBuildRangeInfoKHR, GeometryCount> as_range;

			for (size_t i = 0u; i < GeometryCount; i++) {
				const auto [current_geo, trans_addr, trans_offset] = geometry[i];
				current_geo->accelerationStructureGeometry(as_geometry[i], trans_addr);
				current_geo->accelerationStructureRange(as_range[i], trans_offset);
			}
			
			return AccelStructManager::buildAccelStruct({
				.Device = ctx.Device,
				.Allocator = ctx.Allocator,
				.Command = cmd,
				
				.Type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
				.Flag = flag,

				.CompactionSizeQuery = query_info
			}, as_geometry, as_range);
		}

		/**
		 * @brief Issue a pipeline barrier for geometry data.
		 * @param cmd The command buffer where barrier is issued.
		 * @param src_target The source target.
		 * @param dst_target The destination target.
		*/
		void barrier(VkCommandBuffer, BarrierTarget, BarrierTarget) const;

	};

}