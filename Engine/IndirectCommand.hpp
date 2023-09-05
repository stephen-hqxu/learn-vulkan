#pragma once

#include <cstdint>

namespace LearnVulkan {

	/**
	 * @brief Vulkan indirect draw command memory layout.
	*/
	namespace IndirectCommand {

		/**
		 * @brief vkCmdDrawIndexedIndirect
		*/
		struct VkDrawIndexedIndirectCommand {

			uint32_t    IndexCount;
			uint32_t    InstanceCount;
			uint32_t    FirstIndex;
			int32_t     VertexOffset;
			uint32_t    FirstInstance;

		};

	}

}