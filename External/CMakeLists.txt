add_library(${LV_EXTERNAL} STATIC
	stb_image.h

	VulkanLoader-C.c
	VulkanLoader-CXX.cpp
)
setTargetOutputName(${LV_EXTERNAL})
setupSourceGroup(${LV_EXTERNAL})

target_link_libraries(${LV_EXTERNAL}
	PUBLIC Vulkan::Headers
)