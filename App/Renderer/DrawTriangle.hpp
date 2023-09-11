#pragma once

#include "../Engine/RendererInterface.hpp"
#include "../Engine/VulkanContext.hpp"

#include "../Engine/Abstraction/CommandBufferManager.hpp"
#include "../Engine/Abstraction/DescriptorBufferManager.hpp"
#include "../Engine/Abstraction/FramebufferManager.hpp"
#include "../Engine/Abstraction/ImageManager.hpp"

#include "../Common/VulkanObject.hpp"

#include <glm/mat4x4.hpp>

#include <ostream>
#include <span>

namespace LearnVulkan {

	/**
	 * @brief This is our the very first Vulkan rendering program! Let's draw a triangle.
	*/
	class DrawTriangle final : public RendererInterface {
	private:

		FramebufferManager::SimpleFramebuffer OutputAttachment;
		VkExtent2D OutputExtent;

		const VulkanObject::BufferAllocation VertexBuffer, VertexShaderInstanceOffset;
		struct {

			VulkanObject::ImageAllocation Image;
			VulkanObject::ImageView ImageView;
			VulkanObject::Sampler Sampler;

		} Texture;

		const VulkanObject::DescriptorSetLayout TriangleShaderLayout;

		const VulkanObject::PipelineLayout PipelineLayout;
		const VulkanObject::Pipeline Pipeline;

		const CommandBufferManager::InFlightCommandBufferArray TriangleDrawCmd;
		const VulkanObject::CommandBuffer TriangleReshapeCmd;
		/**
		 * set 1
		 * binding 0: fragment shader sampler
		 * binding 1: vertex shader SSBO
		*/
		DescriptorBufferManager TriangleShaderDescriptorBuffer;

		double CurrentAngle;

		VkDevice getDevice() const noexcept;
		VmaAllocator getAllocator() const noexcept;

		/**
		 * @brief Animate the triangle.
		 * @param delta The delta frame time.
		 * @return The model matrix.
		*/
		glm::mat4 animateTriangle(double) noexcept;

	public:

		/**
		 * @brief Information to create a triangle renderer.
		*/
		struct TriangleCreateInfo {

			VkDescriptorSetLayout CameraDescriptorSetLayout;

			/**
			 * @brief The image data to be displaced on the surface of the triangle.
			 * This information is not retained and can be destroyed after the triangle renderer has been initialised.
			*/
			const ImageManager::ImageReadResult* SurfaceTexture;
			std::ostream* DebugMessage;/**< Must NOT be null and its lifetime should be retained. */

		};

		DrawTriangle(const VulkanContext&, const TriangleCreateInfo&);

		DrawTriangle(const DrawTriangle&) = delete;

		DrawTriangle(DrawTriangle&&) = delete;

		DrawTriangle& operator=(const DrawTriangle&) = delete;

		DrawTriangle& operator=(DrawTriangle&&) = delete;

		~DrawTriangle() override = default;

		void reshape(const ReshapeInfo&) override;

		DrawResult draw(const DrawInfo&) override;

	};

}