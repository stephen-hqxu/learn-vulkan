#pragma once

#include "DrawSky.hpp"
#include "GeometryData.hpp"
#include "PlaneGeometry.hpp"

#include "../Engine/CameraInterface.hpp"
#include "../Engine/RendererInterface.hpp"
#include "../Engine/VulkanContext.hpp"

#include "../Engine/Abstraction/AccelStructManager.hpp"
#include "../Engine/Abstraction/CommandBufferManager.hpp"
#include "../Engine/Abstraction/DescriptorBufferManager.hpp"
#include "../Engine/Abstraction/FramebufferManager.hpp"
#include "../Engine/Abstraction/ImageManager.hpp"

#include "../Common/VulkanObject.hpp"

#include <glm/mat4x4.hpp>

#include <ostream>

namespace LearnVulkan {

	/**
	 * @brief Demonstration of real-time rendering of water reflection and refraction using ray-tracing.
	 * The water renderer must not be used as a stand-alone renderer, hence water will be drawn to an existing framebuffer.
	*/
	class SimpleWater final {
	public:

		/**
		 * @brief Control the rendering output format.
		*/
		struct DrawFormat {

			VkFormat ColourFormat, DepthFormat;
			VkSampleCountFlagBits Sample;

		};

		/**
		 * @brief Information to create a simple water renderer.
		*/
		struct WaterCreateInfo {

			//Some barrier stage and access flags for input data.
			constexpr static VkPipelineStageFlagBits2 GASStage = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
			constexpr static VkAccessFlagBits2 GASAccess = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR;
			constexpr static VkPipelineStageFlagBits2 TextureStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
			constexpr static VkAccessFlagBits2 TextureAccess = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;

			VkDescriptorSetLayout CameraDescriptorSetLayout;
			DrawFormat OutputFormat;

			const DrawSky* SkyRenderer;
			const PlaneGeometry* PlaneGenerator;/**< Provide an existing plane geometry generator to avoid rebuilding the pipeline. */
			/**
			 * @brief The geometry acceleration structure (a.k.a., BLAS) of the scene.
			 * Currently only a single GAS is supported.
			 * An IAS will be built referencing this GAS,
			 * thus a pipeline barrier must be issued by the application prior to construction of water renderer.
			*/
			VkAccelerationStructureKHR SceneGAS;
			VkDescriptorImageInfo SceneTexture;

			const ImageManager::ImageReadResult* WaterNormalmap, *WaterDistortion;

			const glm::mat4* ModelMatrix;

			std::ostream* DebugMessage;

		};

		/**
		 * @brief Information for the application to render onto the scene depth buffer.
		*/
		struct SceneDepthRecordInfo {

			VkPipelineStageFlags2 Stage;
			VkAccessFlags2 Access;
			VkImageLayout Layout;

		};

	private:

		const VkFormat DepthFormat;

		GeometryData WaterSurface;
		AccelStructManager::AccelStruct SceneAccelStruct;
		const VulkanObject::BufferAllocation UniformBuffer;
		VulkanObject::Sampler TextureSampler, SceneDepthSampler;
		struct {

			VulkanObject::ImageAllocation Image;
			VulkanObject::ImageView ImageView;

		} Normalmap, Distortion, SceneDepth;

		const VulkanObject::DescriptorSetLayout WaterShaderLayout;
		const VulkanObject::PipelineLayout PipelineLayout;
		const VulkanObject::Pipeline Pipeline;

		const CommandBufferManager::InFlightCommandBufferArray WaterCommand;
		DescriptorBufferManager WaterShaderDescriptorBuffer;

		mutable double Animator;

		VkDevice getDevice() const noexcept;
		VmaAllocator getAllocator() const noexcept;

	public:

		/**
		 * @brief Information to invoke the water renderer.
		*/
		struct DrawInfo {

			const RendererInterface::DrawInfo* InheritedDrawInfo;
			//The geometry data of the scene.
			//Must be a compatible geometry as the scene acceleration structure
			//passed during construction of water renderer.
			const GeometryData* SceneGeometry;

			//This is the framebuffer to be rendered onto.
			//Content in this framebuffer will be preserved after water renderer finishes.
			const FramebufferManager::SimpleFramebuffer* InputFramebuffer;
			VkImageLayout DepthLayout;

		};

		/**
		 * @brief Construct a simple water renderer.
		 * @param ctx The context.
		 * @param water_info The water renderer create info.
		*/
		SimpleWater(const VulkanContext&, const WaterCreateInfo&);

		SimpleWater(const SimpleWater&) = delete;

		SimpleWater(SimpleWater&&) = delete;

		SimpleWater& operator=(const SimpleWater&) = delete;

		SimpleWater& operator=(SimpleWater&&) = delete;

		~SimpleWater() = default;

		/**
		 * @brief Get the image view to the scene depth texture.
		 * @return The scene depth image view.
		*/
		VkImageView getSceneDepth() const noexcept;

		/**
		 * @brief Begin recording to scene depth.
		 * Barrier and layout transition are performed automatically.
		 * @param cmd The command buffer.
		 * @param record_info Information of recording.
		*/
		void beginSceneDepthRecord(VkCommandBuffer, const SceneDepthRecordInfo&) const noexcept;

		/**
		 * @brief End recording to scene depth.
		 * @param cmd The command buffer.
		 * @param record_info Information of recording.
		*/
		void endSceneDepthRecord(VkCommandBuffer, const SceneDepthRecordInfo&) const noexcept;

		/**
		 * @brief Reshape the internal framebuffer.
		 * @param reshape_info The reshape info.
		*/
		void reshape(const RendererInterface::ReshapeInfo&);

		/**
		 * @brief Draw simple water plane.
		 * The water renderer will draw the final rendered image to resolve output.
		 * @param draw_info The draw information.
		 * @return The draw result.
		 * Specifically, the command buffer returned is a secondary command buffer.
		*/
		RendererInterface::DrawResult draw(const DrawInfo&) const;

	};

}