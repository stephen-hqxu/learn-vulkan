#pragma once

#include "../Engine/CameraInterface.hpp"
#include "../Engine/RendererInterface.hpp"
#include "../Engine/VulkanContext.hpp"

#include "../Engine/Abstraction/CommandBufferManager.hpp"
#include "../Engine/Abstraction/DescriptorBufferManager.hpp"
#include "../Engine/Abstraction/FramebufferManager.hpp"
#include "../Engine/Abstraction/ImageManager.hpp"

#include "../Common/VulkanObject.hpp"

#include <ostream>

namespace LearnVulkan {

	/**
	 * @brief Draw a sky using a cubemap texture.
	 * This renderer does not allocate a framebuffer (thus does not own any rendering memory),
	 * hence it should be used with a primary renderer.
	*/
	class DrawSky final {
	public:

		/**
		 * @brief Specify the format of the input framebuffer.
		*/
		struct DrawFormat {

			VkFormat ColourFormat, DepthFormat;
			VkSampleCountFlagBits Sample;

		};

		/**
		 * @brief Information to create a sky renderer.
		*/
		struct SkyCreateInfo {

			VkDescriptorSetLayout CameraDescriptorSetLayout;
			DrawFormat OutputFormat;

			const ImageManager::ImageReadResult* Cubemap;/**< The cubemap texture containing the sky to be drawn. */

			std::ostream* DebugMessage;

		};

		/**
		 * @brief The information to draw the sky.
		*/
		struct DrawInfo {
		
			const RendererInterface::DrawInfo* InheritedDrawInfo;
			//Where sky will be rendered onto.
			//Note that sky should be the last renderer invoked in a frame,
			//as all contents in this framebuffer become undefined after renderer finishes,
			//and final colour will be written to the present image.
			const FramebufferManager::SimpleFramebuffer* InputFramebuffer;

			VkImageLayout DepthLayout;
		
		};

	private:

		struct {

			VulkanObject::ImageAllocation Image;
			VulkanObject::ImageView ImageView;
			VulkanObject::Sampler Sampler;

		} SkyBox;

		const VulkanObject::BufferAllocation SkyIndirectCommand;

		const VulkanObject::DescriptorSetLayout SkyShaderLayout;
		const VulkanObject::PipelineLayout PipelineLayout;
		const VulkanObject::Pipeline Pipeline;

		const CommandBufferManager::InFlightCommandBufferArray SkyCommand;
		DescriptorBufferManager SkyShaderDescriptorBuffer;

		VkDevice getDevice() const noexcept;

	public:

		/**
		 * @brief Initialise a sky renderer.
		 * @param ctx The Vulkan context.
		 * @param sky_info The information to create a sky renderer.
		*/
		DrawSky(const VulkanContext&, const SkyCreateInfo&);

		DrawSky(const DrawSky&) = delete;

		DrawSky(DrawSky&&) = delete;

		DrawSky& operator=(const DrawSky&) = delete;

		DrawSky& operator=(DrawSky&&) = delete;

		~DrawSky() = default;

		/**
		 * @brief Get the descriptor data of sky image.
		 * @return The sky image descriptor.
		*/
		VkDescriptorImageInfo skyImageDescriptor() const noexcept;

		/**
		 * @brief Draw sky.
		 * @param draw_info Draw information.
		 * @return The draw result.
		*/
		RendererInterface::DrawResult draw(const DrawInfo&) const;
	
	};

}