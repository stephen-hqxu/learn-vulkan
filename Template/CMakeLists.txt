include(DownloadResource)

set(LV_TEXTURE_FILENAME "Texture-v1")
downloadResource(
	URL "https://github.com/stephen-hqxu/learn-vulkan/releases/download/r1/${LV_TEXTURE_FILENAME}.zip"
	FILENAME ${LV_TEXTURE_FILENAME}
	EXTENSION ".zip"
	LOCATION LV_TEXTURE_DIR
	EXTRACT
)

##########################
### Generate config file
##########################
set(LV_TEMPLATE_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/TemplateInclude")
set(LV_SHADER_ROOT "${CMAKE_SOURCE_DIR}/${LV_MAIN}/Shader")
set(LV_RESOURCE_ROOT "${LV_TEXTURE_DIR}/Texture")

function(generateTemplateConfig OutputFilename)
	configure_file(
		"${CMAKE_CURRENT_SOURCE_DIR}/${OutputFilename}.in"
		"${LV_TEMPLATE_INCLUDE_DIR}/${PROJECT_NAME}/GeneratedTemplate/${OutputFilename}"
	)
endfunction()

generateTemplateConfig("ResourcePath.hpp")

add_library(${LV_TEMPLATE} INTERFACE)
target_include_directories(${LV_TEMPLATE} INTERFACE ${LV_TEMPLATE_INCLUDE_DIR})