find_package(Vulkan 1.3 REQUIRED)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#######################
### Definitions
#######################
add_compile_definitions(
	VK_NO_PROTOTYPES
	VMA_STATIC_VULKAN_FUNCTIONS=0
	VMA_DYNAMIC_VULKAN_FUNCTIONS=1
	GLM_FORCE_DEPTH_ZERO_TO_ONE
)

if(MSVC)
	add_compile_options(/W4 /arch:AVX2 /MP)

	add_library(Vulkan::shaderc_shared SHARED IMPORTED)
	cmake_path(GET Vulkan_LIBRARY PARENT_PATH VULKAN_LIB_PATH)
	set_target_properties(Vulkan::shaderc_shared PROPERTIES IMPORTED_IMPLIB "${VULKAN_LIB_PATH}/shaderc_shared.lib")
	unset(VULKAN_LIB_PATH)
else()
	message(FATAL_ERROR "Compilers other than MSVC are not tested, thus unsupported.")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/arc)