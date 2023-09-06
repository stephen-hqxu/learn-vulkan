#pragma once

#include "../../Common/VulkanObject.hpp"

#include <optional>
#include <span>

namespace LearnVulkan {

	/**
	 * @brief A fast way to compile graphics and compute pipeline.
	*/
	namespace PipelineManager {
	
		//Members are pretty self-explanatory.
		struct SimpleGraphicsPipelineCreateInfo {

			std::span<const VkPipelineShaderStageCreateInfo> ShaderStage;
			const VkPipelineVertexInputStateCreateInfo* VertexInputState;
			const VkPipelineRenderingCreateInfo* Rendering;

			VkPrimitiveTopology PrimitiveTopology;

			VkCullModeFlags CullMode = VK_CULL_MODE_BACK_BIT;
			VkFrontFace FrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

			VkSampleCountFlagBits Sample;
			std::optional<float> MinSampleShading;

			//Use no blending if left empty.
			std::span<const VkPipelineColorBlendAttachmentState> Blending;

			struct {

				bool Colour = false, Depth = false;

			} AllowFeedbackLoop;

		};

		/**
		 * @brief Create a simple graphics pipeline.
		 * - Tessellation patch size is set to be 3.
		 * - Only scissor and viewport dynamic states are supported.
		 * @param device The device.
		 * @param layout The pipeline layout.
		 * @param graphics_info The simple graphics pipeline create info.
		 * @return Graphics pipeline.
		*/
		VulkanObject::Pipeline createSimpleGraphicsPipeline(VkDevice, VkPipelineLayout, const SimpleGraphicsPipelineCreateInfo&);
	
	}

}