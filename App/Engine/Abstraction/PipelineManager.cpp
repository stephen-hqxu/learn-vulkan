#include "PipelineManager.hpp"

#include <array>

using std::array;

using namespace LearnVulkan;
namespace VKO = VulkanObject;

namespace {

	consteval VkPipelineCreateFlags getCommonPipelineFlag() noexcept {
		return VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
#ifndef NDEBUG
			| VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT
#endif
		;
	}

}

VKO::Pipeline PipelineManager::createSimpleGraphicsPipeline(const VkDevice device, const VkPipelineLayout layout,
	const SimpleGraphicsPipelineCreateInfo& graphics_info) {
	const auto& [shader_stage, vertex_input_state, rendering,
		primitive_topo, cull_move, front_face, sample, min_sample_shading, depth, blend_state, allow_feedback_loop] = graphics_info;
	const auto [depth_write, depth_compare] = depth;
	const auto [colour_fbl, depth_fbl] = allow_feedback_loop;

	/////////////////////
	/// Vertex attribute
	/////////////////////
	constexpr static VkPipelineVertexInputStateCreateInfo attribute_less {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
	};

	///////////////////////
	/// Input assembly
	//////////////////////
	const VkPipelineInputAssemblyStateCreateInfo input_assembly {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = primitive_topo,
		.primitiveRestartEnable = VK_FALSE
	};

	//////////////////
	/// Tessellation
	/////////////////
	constexpr static VkPipelineTessellationStateCreateInfo tess {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
		.patchControlPoints = 3u
	};

	////////////////
	/// Viewport
	////////////////
	constexpr static VkPipelineViewportStateCreateInfo vp {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1u,
		.scissorCount = 1u
	};

	////////////////////
	/// Rasterisation
	////////////////////
	const VkPipelineRasterizationStateCreateInfo rasterisation {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = cull_move,
		.frontFace = front_face,
		.lineWidth = 1.0f
	};

	////////////////////
	/// Multi-sampling
	////////////////////
	const VkPipelineMultisampleStateCreateInfo ms {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = sample,
		.sampleShadingEnable = min_sample_shading ? VK_TRUE : VK_FALSE,
		.minSampleShading = min_sample_shading.value_or(0.0f)
	};

	////////////////////
	/// Fragment test
	///////////////////
	const VkPipelineDepthStencilStateCreateInfo depth_stencil {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		.depthWriteEnable = depth_write ? VK_TRUE : VK_FALSE,
		.depthCompareOp = static_cast<VkCompareOp>(depth_compare)
	};

	//////////////////////
	/// Blending
	/////////////////////
	const bool custom_blending = !blend_state.empty();
	constexpr static VkPipelineColorBlendAttachmentState atm_blend {
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
	};
	const VkPipelineColorBlendStateCreateInfo blending {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = custom_blending ? static_cast<uint32_t>(blend_state.size()) : 1u,
		.pAttachments = custom_blending ? blend_state.data() : &atm_blend
	};

	///////////////////
	/// Dynamic state
	///////////////////
	constexpr static auto dynamic_state = array {
		VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR
	};
	constexpr static VkPipelineDynamicStateCreateInfo dynamic_state_info {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = static_cast<uint32_t>(dynamic_state.size()),
		.pDynamicStates = dynamic_state.data()
	};

	VkPipelineCreateFlags pipeline_flag = getCommonPipelineFlag();
	if (colour_fbl) {
		pipeline_flag |= VK_PIPELINE_CREATE_COLOR_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT;
	}
	if (depth_fbl) {
		pipeline_flag |= VK_PIPELINE_CREATE_DEPTH_STENCIL_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT;
	}

	return VKO::createGraphicsPipeline(device, VK_NULL_HANDLE, {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext = rendering,
		.flags = pipeline_flag,
		.stageCount = static_cast<uint32_t>(shader_stage.size()),
		.pStages = shader_stage.data(),
		.pVertexInputState = vertex_input_state ? vertex_input_state : &attribute_less,
		.pInputAssemblyState = &input_assembly,
		.pTessellationState = &tess,
		.pViewportState = &vp,
		.pRasterizationState = &rasterisation,
		.pMultisampleState = &ms,
		.pDepthStencilState = &depth_stencil,
		.pColorBlendState = &blending,
		.pDynamicState = &dynamic_state_info,
		.layout = layout
	});
}