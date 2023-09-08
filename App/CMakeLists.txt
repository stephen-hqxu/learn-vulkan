find_package(glm REQUIRED CONFIG)
find_package(glfw3 3 REQUIRED CONFIG)

add_executable(${LV_MAIN}
	# Common/
	Common/ErrorHandler.cpp
	Common/ErrorHandler.hpp
	Common/File.cpp
	Common/File.hpp
	Common/FixedArray.hpp
	Common/SpanArray.hpp
	Common/StaticArray.hpp
	Common/VulkanObject.cpp
	Common/VulkanObject.hpp
	# Engine/Abstraction/
	Engine/Abstraction/AccelStructManager.cpp
	Engine/Abstraction/AccelStructManager.hpp
	Engine/Abstraction/BufferManager.cpp
	Engine/Abstraction/BufferManager.hpp
	Engine/Abstraction/CommandBufferManager.cpp
	Engine/Abstraction/CommandBufferManager.hpp
	Engine/Abstraction/DescriptorBufferManager.cpp
	Engine/Abstraction/DescriptorBufferManager.hpp
	Engine/Abstraction/FramebufferManager.cpp
	Engine/Abstraction/FramebufferManager.hpp
	Engine/Abstraction/ImageManager.cpp
	Engine/Abstraction/ImageManager.hpp
	Engine/Abstraction/PipelineBarrier.hpp
	Engine/Abstraction/PipelineManager.cpp
	Engine/Abstraction/PipelineManager.hpp
	Engine/Abstraction/SemaphoreManager.cpp
	Engine/Abstraction/SemaphoreManager.hpp
	Engine/Abstraction/ShaderModuleManager.cpp
	Engine/Abstraction/ShaderModuleManager.hpp
	# Engine/
	Engine/Camera.cpp
	Engine/Camera.hpp
	Engine/CameraInterface.hpp
	Engine/ContextManager.cpp
	Engine/ContextManager.hpp
	Engine/EngineSetting.hpp
	Engine/IndirectCommand.hpp
	Engine/MasterEngine.cpp
	Engine/MasterEngine.hpp
	Engine/RendererInterface.hpp
	Engine/VulkanContext.hpp
	# Renderer/
	Renderer/DrawTriangle.cpp
	Renderer/DrawTriangle.hpp
	Renderer/GeometryData.cpp
	Renderer/GeometryData.hpp
	Renderer/PlaneGeometry.cpp
	Renderer/PlaneGeometry.hpp
	Renderer/SimpleTerrain.cpp
	Renderer/SimpleTerrain.hpp
	Renderer/SimpleWater.cpp
	Renderer/SimpleWater.hpp
	# Shader/
	Shader/CameraData.glsl
	Shader/DrawTriangle.frag
	Shader/DrawTriangle.vert
	Shader/PlaneDisplacer.comp
	Shader/PlaneGenerator.comp
	Shader/PlaneGeometry.glsl
	Shader/PlaneGeometryAttribute.glsl
	Shader/SimpleTerrain.frag
	Shader/SimpleTerrain.tesc
	Shader/SimpleTerrain.tese
	Shader/SimpleTerrain.vert
	Shader/SimpleWater.frag
	Shader/SimpleWater.glsl
	Shader/SimpleWater.vert
	# /
	Start.cpp
)
setTargetOutputName(${LV_MAIN})
setupSourceGroup(${LV_MAIN})

if(MSVC)
	set_target_properties(${LV_MAIN} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY $<TARGET_FILE_DIR:${LV_MAIN}>)
endif()

target_link_libraries(${LV_MAIN}
	PUBLIC ${LV_TEMPLATE}
	PRIVATE ${LV_EXTERNAL} Vulkan::shaderc_shared
	PRIVATE glm::glm glfw
)