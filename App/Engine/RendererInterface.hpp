#pragma once

#include "VulkanContext.hpp"
#include "CameraInterface.hpp"

#include <Volk/volk.h>

namespace LearnVulkan {

	/**
	 * @brief Provide a generic interface for different rendering class.
	 * This interface is made to be immutable.
	*/
	class RendererInterface {
	public:

		/**
		 * @brief Information about output reshape.
		*/
		struct ReshapeInfo {

			const VulkanContext* Context;

			VkExtent2D Extent;

		};

		/**
		 * @brief Data used for draw command.
		*/
		struct DrawInfo {

			const VulkanContext* Context;

			const CameraInterface* Camera;

			double DeltaTime;/**< The frame time from last time the draw function is called. */
			unsigned int FrameInFlightIndex;/**< sub-frame index */

			VkViewport Viewport;
			VkRect2D DrawArea;

			/**
			 * @brief The image where the rendering output should be written to.
			 * The final layout of this image must be transitioned and ready for presentation.
			*/
			VkImage PresentImage;
			VkImageView PresentImageView;

		};

		/**
		 * @brief Result after issuing draw command.
		*/
		struct DrawResult {

			VkCommandBuffer DrawCommand;
			VkPipelineStageFlags2 WaitStage;

		};

		/**
		 * @brief Initialise the renderer.
		 * Renderer initialisation is at the beginning of the main rendering loop, after all Vulkan context has been setup.
		*/
		constexpr RendererInterface() noexcept = default;

		RendererInterface(const RendererInterface&) = delete;

		RendererInterface(RendererInterface&&) = delete;

		RendererInterface& operator=(const RendererInterface&) = delete;

		RendererInterface& operator=(RendererInterface&&) = delete;

		virtual ~RendererInterface() = default;

		/**
		 * @brief Trigger a output buffer reshape.
		 * @param reshape_info The information about this reshape.
		*/
		virtual void reshape(const ReshapeInfo&) = 0;

		/**
		 * @brief Draw stuff on the canvas in the current rendering context.
		 * This function is called once in every rendering loop.
		 * @param draw_info The information used for the draw command.
		 * @return The draw result.
		*/
		virtual DrawResult draw(const DrawInfo&) = 0;

	};

}