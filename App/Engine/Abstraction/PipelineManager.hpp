#pragma once

#include "../../Common/VulkanObject.hpp"

#include <optional>
#include <span>
#include <type_traits>

namespace LearnVulkan {

	/**
	 * @brief A fast way to compile graphics and compute pipeline.
	*/
	namespace PipelineManager {

		/**
		 * @brief Specify the depth comparison mode.
		*/
		enum class DepthComparator : std::underlying_type_t<VkCompareOp> {
			Default = VK_COMPARE_OP_GREATER,
			DefaultOrEqual = VK_COMPARE_OP_GREATER_OR_EQUAL
		};
	
		/**
		 * @brief Members are pretty self-explanatory.
		 * Unless otherwise specified, all fields must be non-empty and not null.
		*/
		struct SimpleGraphicsPipelineCreateInfo {

			std::span<const VkPipelineShaderStageCreateInfo> ShaderStage;
			//Can be null to use attribute-less rendering.
			const VkPipelineVertexInputStateCreateInfo* VertexInputState = nullptr;
			const VkPipelineRenderingCreateInfo* Rendering;

			VkPrimitiveTopology PrimitiveTopology;

			VkCullModeFlags CullMode = VK_CULL_MODE_BACK_BIT;
			VkFrontFace FrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

			VkSampleCountFlagBits Sample = VK_SAMPLE_COUNT_1_BIT;
			std::optional<float> MinSampleShading;

			struct {

				bool Write = true;
				DepthComparator Comparator = DepthComparator::Default;

			} Depth;

			//Can be an empty span to use no blending.
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