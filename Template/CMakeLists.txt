include(DownloadResource)

function(downloadAssetResource ReleaseTagName Filename DownloadedLocation)
	downloadResource(
		URL "https://github.com/stephen-hqxu/learn-vulkan/releases/download/${ReleaseTagName}/${Filename}.tar.gz"
		FILENAME ${Filename}
		EXTENSION ".tar.gz"
		LOCATION ${DownloadedLocation}
		EXTRACT
	)
	set(${DownloadedLocation} ${${DownloadedLocation}} PARENT_SCOPE)
endfunction()

downloadAssetResource("r2" "GeneralAsset-r2" LV_GENERAL_RESOURCE_ROOT)
downloadAssetResource("r2" "Heightfield-Texture-Sample-r2" LV_HEIGHTFIELD_RESOURCE_ROOT)
downloadAssetResource("r2" "SkyCubeMap-r2" LV_SKY_CUBEMAP_RESOURCE_ROOT)

##########################
### Generate config file
##########################
set(LV_TEMPLATE_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/TemplateInclude")
set(LV_SHADER_ROOT "${CMAKE_SOURCE_DIR}/${LV_MAIN}/Shader")

function(generateTemplateConfig OutputFilename)
	configure_file(
		"${CMAKE_CURRENT_SOURCE_DIR}/${OutputFilename}.in"
		"${LV_TEMPLATE_INCLUDE_DIR}/${PROJECT_NAME}/GeneratedTemplate/${OutputFilename}"
	)
endfunction()

generateTemplateConfig("ResourcePath.hpp")

add_library(${LV_TEMPLATE} INTERFACE)
target_include_directories(${LV_TEMPLATE} INTERFACE ${LV_TEMPLATE_INCLUDE_DIR})