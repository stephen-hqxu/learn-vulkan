cmake_minimum_required(VERSION 3.21 FATAL_ERROR)

project(LearnVulkan
	LANGUAGES C CXX
	DESCRIPTION "My toy project for personal learning and exploring latest features in Vulkan"
	HOMEPAGE_URL "https://github.com/stephen-hqxu/learn-vulkan"
)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/CMake)
include(SetupEnvironment)

set(LV_MAIN "App")
set(LV_TEMPLATE "Template")
set(LV_EXTERNAL "External")

function(setupSourceGroup BuildTarget)
	get_target_property(TargetSource ${BuildTarget} SOURCES)

	source_group(
		TREE ${CMAKE_CURRENT_SOURCE_DIR}
		FILES ${TargetSource}
	)
endfunction()
function(setTargetOutputName TargetName)
	set_target_properties(${TargetName} PROPERTIES PREFIX "${PROJECT_NAME}")
endfunction()

add_subdirectory(${LV_EXTERNAL})
add_subdirectory(${LV_TEMPLATE})
add_subdirectory(${LV_MAIN})