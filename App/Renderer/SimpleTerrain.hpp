#pragma once

#include "DrawSky.hpp"
#include "SimpleWater.hpp"
#include "GeometryData.hpp"

#include "../Engine/RendererInterface.hpp"
#include "../Engine/VulkanContext.hpp"

#include "../Engine/Abstraction/AccelStructManager.hpp"
#include "../Engine/Abstraction/CommandBufferManager.hpp"
#include "../Engine/Abstraction/DescriptorBufferManager.hpp"
#include "../Engine/Abstraction/FramebufferManager.hpp"
#include "../Engine/Abstraction/ImageManager.hpp"

#include "../Common/VulkanObject.hpp"

#include <ostream>
#include <optional>
#include <span>
#include <tuple>
#include <cstdint>

namespace LearnVulkan {

	/**
	 * @brief Demonstration of terrain rendering using pre-generated 2D heightmap and tessellation shader.
	*/
	class SimpleTerrain final : public RendererInterface {
	private:

		using AccelStructBuildTempMemory = std::tuple<VulkanObject::BufferAllocation, VulkanObject::BufferAllocation>;

		FramebufferManager::SimpleFramebuffer OutputAttachment;
		VkExtent2D OutputExtent;

		GeometryData Plane, AccelStructPlane;
		const VulkanObject::BufferAllocation UniformBuffer;
		struct {

			VulkanObject::ImageAllocation Image;
			VulkanObject::ImageView FullView, DisplacementSwizzleView, NormalOnlyView;
			VulkanObject::Sampler Sampler;

		} Heightfield;

		const VulkanObject::DescriptorSetLayout TerrainShaderLayout;

		const VulkanObject::PipelineLayout PipelineLayout;
		const VulkanObject::Pipeline Pipeline;

		const CommandBufferManager::InFlightCommandBufferArray TerrainDrawCmd;
		const VulkanObject::CommandBuffer TerrainReshapeCmd;
		/**
		 * @brief set 1
		 * binding 0: vertex SSBO
		 * binding 1: tessellation control SSBO
		 * binding 2: tessellation evaluation SSBO
		 * binding 3: heightfield
		*/
		DescriptorBufferManager TerrainShaderDescriptorBuffer;

		//The following fields are used by water renderer and are hence optional.
		AccelStructManager::AccelStruct TerrainAccelStruct;
		DrawSky SkyRenderer;
		std::optional<SimpleWater> WaterRenderer;

		VkDevice getDevice() const noexcept;
		VmaAllocator getAllocator() const noexcept;

		constexpr AccelStructManager::CompactionSizeQueryInfo createCompactionQueryInfo(VkQueryPool) const noexcept;

		//Record command to build an acceleration structure for the terrain geometry.
		//Returns some temporary buffer that must be preserved until build operation is finished.
		AccelStructBuildTempMemory buildTerrainAccelStruct(const VulkanContext&, VkCommandBuffer, VkQueryPool);

		//Record command to compact an acceleration structure for the terrain geometry.
		//Return the compacted acceleration structure.
		AccelStructManager::AccelStruct compactTerrainAccelStruct(VkCommandBuffer, VkQueryPool) const;

	public:

		/**
		 * @brief Information to create a sky renderer.
		*/
		struct TerrainSkyCreateInfo {

			const ImageManager::ImageReadResult* SkyBox;

		};

		/**
		 * @brief Information to create a water renderer.
		*/
		struct TerrainWaterCreateInfo {

			const ImageManager::ImageReadResult* WaterNormalmap, *WaterDistortion;

		};

		/**
		 * @brief Information to create a terrain renderer.
		*/
		struct TerrainCreateInfo {
		
			VkDescriptorSetLayout CameraDescriptorSetLayout;

			const TerrainSkyCreateInfo* SkyInfo;
			/**
			 * @brief Set to null to disable water rendering.
			 * Require support for acceleration structure and ray query.
			*/
			const TerrainWaterCreateInfo* WaterInfo = nullptr;
			//The heightfield should contains RGB as normalmap and A as displacementmap.
			const ImageManager::ImageReadResult* Heightfield;
			std::ostream* DebugMessage;

		};

		SimpleTerrain(const VulkanContext&, const TerrainCreateInfo&);

		SimpleTerrain(const SimpleTerrain&) = delete;

		SimpleTerrain(SimpleTerrain&&) = delete;

		SimpleTerrain& operator=(const SimpleTerrain&) = delete;

		SimpleTerrain& operator=(SimpleTerrain&&) = delete;

		~SimpleTerrain() = default;

		void reshape(const ReshapeInfo&) override;

		DrawResult draw(const DrawInfo&) override;

	};

}