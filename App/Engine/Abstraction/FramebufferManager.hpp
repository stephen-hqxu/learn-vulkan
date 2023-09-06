#pragma once

#include "../../Common/VulkanObject.hpp"

#include <array>
#include <optional>

#include <glm/vec4.hpp>

namespace LearnVulkan {

	/**
	 * @brief Manage creation and lifetime of framebuffer.
	*/
	namespace FramebufferManager {
	
		struct SimpleFramebufferCreateInfo {

			VkDevice Device;
			VmaAllocator Allocator;
			VkFormat ColourFormat, DepthFormat;
			VkSampleCountFlagBits Sample;

			VkExtent2D Extent;

		};

		struct SimpleFramebuffer {

			/**
			 * @brief There is no need to create one layer for each in-flight frame.
			 * The whole point of having in-flight frame is to ensure CPU is no idling while GPU the in execution.
			 * 
			 * As we are not rendering 2 frames at the same time (because we only have one GPU),
			 * framebuffer is cleared at the beginning of each frame. And because of use of pipeline barrier,
			 * we can ensure the next frame does not start before the previous one has finished.
			*/
			VulkanObject::ImageAllocation Colour, Depth;

			struct Attachment_t {

				VulkanObject::ImageView ColourView, DepthView;/**< View into each layer. */
				//Frame buffer object is not provided as we are using dynamic rendering only.

			} Attachment;

		};

		struct PrepareFramebufferInfo {

			VkImageLayout DepthLayout;/**< The intended depth image layout. */

		};

		struct SubpassOutputDependencyIssueInfo {

			const PrepareFramebufferInfo* PrepareInfo;

			//If resolve output is null, then no subpass output dependency is issued for this image.
			VkImage ResolveOutput = VK_NULL_HANDLE;

		};

		struct InitialRenderingBeginInfo {

			const SubpassOutputDependencyIssueInfo* DependencyInfo;

			//If clear colour is not specified, colour and depth attachment will not be cleared.
			//Only specification of clear colour is allowed, clear depth/stencil value is implementation-specified.
			std::optional<glm::vec4> ClearColour;
			VkRect2D RenderArea;

			//Specify resolve output.
			//If null, resolution is not performed.
			//Resolve mode is implementation defined.
			struct {

				VkImageView Colour = VK_NULL_HANDLE,
					Depth = VK_NULL_HANDLE;

			} ResolveOutput;
			//If set to false, data within framebuffer will become undefined after rendering.
			//Useful for the last rendering call in a frame.
			//The default value depends on resolve output. If resolve output is given,
			//then the corresponding component is false; otherwise, it will be true.
			struct {

				std::optional<bool> Colour, Depth;

			} RequiredAfterRendering;

		};

		/**
		 * @brief Create a simple frame buffer that contains one colour and depth attachment.
		 * @param fbo_info The information to create the framebuffer.
		 * @return The created simple framebuffer.
		*/
		SimpleFramebuffer createSimpleFramebuffer(const SimpleFramebufferCreateInfo&);

		/**
		 * @brief Perform image layout transition for simple framebuffer to prepare for rendering.
		 * @param cmd The command buffer for recording.
		 * @param fbo The framebuffer to be transitioned.
		 * @param prepare_info The information about prepare framebuffer.
		*/
		void prepareFramebuffer(VkCommandBuffer, const SimpleFramebuffer&, const PrepareFramebufferInfo&);

		/**
		 * @brief Issue image barriers for simple framebuffer before rendering.
		 * Depth buffer is assumed to have early depth testing enabled.
		 * @param cmd The command buffer.
		 * @param fbo The framebuffer whose dependency barriers are issued.
		 * @param issue_info The information of the dependency.
		*/
		void issueSubpassOutputDependency(VkCommandBuffer, const SimpleFramebuffer&, const SubpassOutputDependencyIssueInfo&);

		/**
		 * @brief Begin rendering of the first frame, which clears all old data.
		 * @param cmd The command buffer.
		 * @param fbo The framebuffer.
		 * @param rendering_info The information of the initial rendering.
		*/
		void beginInitialRendering(VkCommandBuffer, const SimpleFramebuffer&, const InitialRenderingBeginInfo&);

		/**
		 * @brief Perform layout transition from colour output attachment to present.
		 * @param cmd The command buffer.
		 * @param img The surface image.
		*/
		void transitionAttachmentToPresent(VkCommandBuffer, VkImage);

	}

}