#pragma once

#include <cstdint>

namespace LearnVulkan {

	/**
	 * @brief Vulkan indirect draw command memory layout.
	*/
	namespace IndirectCommand {

		/**
		 * @brief vkCmdDrawIndirect
		*/
		struct VkDrawIndirectCommand {

			uint32_t VertexCount,
				InstanceCount,
				FirstVertex,
				FirstInstance;

		};

		/**
		 * @brief vkCmdDrawIndexedIndirect
		*/
		struct VkDrawIndexedIndirectCommand {

			uint32_t IndexCount,
				InstanceCount,
				FirstIndex;
			int32_t VertexOffset;
			uint32_t FirstInstance;

		};

	}

}