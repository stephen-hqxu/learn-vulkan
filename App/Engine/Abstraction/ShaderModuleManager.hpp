#pragma once

#include "../../Common/StaticArray.hpp"

#include <Volk/volk.h>
#include <shaderc/shaderc.hpp>

#include <string_view>
#include <array>
#include <span>

#include <coroutine>
#include <ostream>

#include <cstdint>

namespace LearnVulkan {

	/**
	 * @brief A simple framework to runtime-compile shader from source.
	*/
	namespace ShaderModuleManager {

		struct ShaderBatchCompilationInfo;

		namespace _Internal {

			/**
			 * @brief Shader compilation output.
			*/
			struct ShaderOutput {

				StaticArray<uint32_t> Code;
				VkShaderModuleCreateInfo SMInfo;
				VkShaderStageFlagBits Stage;

			};

			//Shader compilation output is allocated automatically on stack.
			void batchShaderCompilation(const ShaderBatchCompilationInfo&, std::ostream&,
				const shaderc::CompileOptions&, std::span<ShaderOutput>);

		}

		struct ShaderOutputView;
		/**
		 * @brief A handy data generator to yield shader output info one by one.
		 * The underlying coroutine handle is destroyed when the generator goes out of scope.
		*/
		struct ShaderOutputGenerator final : std::coroutine_handle<ShaderOutputView> {
		public:

			using promise_type = ShaderOutputView;

			~ShaderOutputGenerator();

		};

		/**
		 * @brief A non-owning view to the shader output.
		*/
		struct ShaderOutputView {

			const VkShaderModuleCreateInfo* SMInfo;
			VkShaderStageFlagBits Stage;

			ShaderOutputView() noexcept;

			/**
			 * @brief Initialise a shader output view.
			 * @param shader_output The shader output.
			*/
			ShaderOutputView(const _Internal::ShaderOutput& shader_output) noexcept;

			~ShaderOutputView() = default;

			ShaderOutputGenerator get_return_object() noexcept;

			std::suspend_never initial_suspend() noexcept;
			std::suspend_never final_suspend() noexcept;
			std::suspend_always yield_value(ShaderOutputView) noexcept;

			constexpr void return_void() noexcept { }
			void unhandled_exception();

		};
	
		struct ShaderBatchCompilationInfo {

			VkDevice Device;
			
			const std::string_view* ShaderFilename;/**< An array of shader filename. */
			const shaderc_shader_kind* ShaderKind;/**< Shader type for each of the input. */

		};

		extern const shaderc::CompileOptions DefaultCompileOption;

		/**
		 * @brief Used for runtime shader compilation, and return commonly used shader options for the program.
		 * @return Common shader option.
		*/
		shaderc::CompileOptions createCommonShaderCompileOption();

		/**
		 * @brief Quickly compile a collection of shader source code to shader module.
		 * None of the input argument are retained after the coroutine handle has been created and returned.
		 * @tparam ShaderCount Specify the number of shader.
		 * @param info Information to batch compile the shader.
		 * @param out The stream output where diagnostic messages are written to.
		 * @param option Compilation options. All shader source will share the same option.
		 * If not given, the common shader compile option will be used.
		 * @return A shader output generator looping through each compiled shader output one by one.
		 * @exception If the shader compilation has resulted in an error.
		*/
		template<size_t ShaderCount>
		inline ShaderOutputGenerator batchShaderCompilation(const ShaderBatchCompilationInfo* const info, std::ostream* const out,
			const shaderc::CompileOptions* const option = &DefaultCompileOption) {
			std::array<_Internal::ShaderOutput, ShaderCount> shader_output;
			_Internal::batchShaderCompilation(*info, *out, *option, shader_output);
			co_await std::suspend_always { };
			//from this point, all input pointers might be dangling

			for (const auto& so : shader_output) {
				co_yield ShaderOutputView(so);
			}
		}
	
	}

}