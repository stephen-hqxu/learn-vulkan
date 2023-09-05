#include "ShaderModuleManager.hpp"

#include "../../Common/File.hpp"

#include <string>
#include <iostream>
#include <sstream>

#include <memory>
#include <filesystem>
#include <utility>

#include <exception>
#include <stdexcept>

using std::string, std::string_view, std::span;
using std::ostringstream, std::ostream, std::endl;
using std::unique_ptr, std::make_unique;
using std::suspend_always, std::suspend_never;
using std::runtime_error, std::move;

using namespace LearnVulkan;

namespace {

	//A custom include utility for shader compilation.
	//Damn, I hate interfacing with C API, I hate using `new` and `delete`.
	class ShaderIncluder final : public shaderc::CompileOptions::IncluderInterface {
	private:

		//Include file source information.
		struct FileInfo {

			string FullPath, Content;

		};

		//Construct shader include result from file info.
		//The shader include result will be owning the file info on return.
		//The shader include result is allocated on the head and should be freed manually.
		static shaderc_include_result* constructIncludeResult(unique_ptr<FileInfo>&& file_info) noexcept {
			const auto& [path, content] = *file_info;
			return new shaderc_include_result { path.c_str(), path.length(), content.c_str(), content.length(), file_info.release() };
		}

	public:

		constexpr ShaderIncluder() noexcept = default;

		~ShaderIncluder() override = default;

		shaderc_include_result* GetInclude(const char* const requested_source, const shaderc_include_type,
			const char* const requesting_source, const size_t) override {
			namespace fs = std::filesystem;

			auto src = make_unique<FileInfo>();
			//convert the requesting source to absolute path relative to the requested source
			fs::path include_filename(requesting_source);
			include_filename.replace_filename(requested_source);
			include_filename = include_filename.lexically_normal();
			if (!fs::exists(include_filename)) {
				using namespace std::string_literals;

				src->Content = "The requesting source \'"s + include_filename.string() + "\' is not found."s;
				return ShaderIncluder::constructIncludeResult(move(src));
			}

			src->FullPath = include_filename.string();
			src->Content = File::readString(src->FullPath.c_str());
			return ShaderIncluder::constructIncludeResult(move(src));
		}

		void ReleaseInclude(shaderc_include_result* const data) override {
			delete reinterpret_cast<FileInfo*>(data->user_data);
			delete data;
		}

	};

	//Process the shader compilation result, and put the result binary to a separate array of memory.
	StaticArray<uint32_t> processShaderCompilationResult(const shaderc::CompilationResult<uint32_t>& result, ostream& out) {
		//compilation status handling
		out << result.GetNumWarnings() << " warning, " << result.GetNumErrors() << " error" << endl;
		if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
			ostringstream err;
			err << "A shader compilation error has encountered!\n";
			err << result.GetErrorMessage() << endl;
			throw runtime_error(err.str());
		}

		//getting compilation binary
		const auto bin_range = std::ranges::subrange(result.cbegin(), result.cend());
		StaticArray<uint32_t> bin(bin_range.size());
		std::ranges::copy(bin_range, bin.data());
		return bin;
	}

	VkShaderStageFlagBits fromKindToStage(const shaderc_shader_kind kind) {
		switch (kind) {
		case shaderc_vertex_shader: return VK_SHADER_STAGE_VERTEX_BIT;
		case shaderc_tess_control_shader: return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
		case shaderc_tess_evaluation_shader: return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
		case shaderc_fragment_shader: return VK_SHADER_STAGE_FRAGMENT_BIT;
		case shaderc_compute_shader: return VK_SHADER_STAGE_COMPUTE_BIT;
		default:
			throw runtime_error("The shader kind is unknown and cannot be converted to shader stage flag.");
		}
	}

}

const shaderc::CompileOptions ShaderModuleManager::DefaultCompileOption = ShaderModuleManager::createCommonShaderCompileOption();

ShaderModuleManager::ShaderOutputGenerator::~ShaderOutputGenerator() {
	if (*this) {
		this->destroy();
	}
}

ShaderModuleManager::ShaderOutputView::ShaderOutputView() noexcept : SMInfo(nullptr), Stage(VK_SHADER_STAGE_ALL) {

}

ShaderModuleManager::ShaderOutputView::ShaderOutputView(const _Internal::ShaderOutput& shader_output) noexcept :
	SMInfo(&shader_output.SMInfo), Stage(shader_output.Stage) {

}

ShaderModuleManager::ShaderOutputGenerator ShaderModuleManager::ShaderOutputView::get_return_object() noexcept {
	return { ShaderOutputGenerator::from_promise(*this) };
}

suspend_never ShaderModuleManager::ShaderOutputView::initial_suspend() noexcept {
	return { };
}

suspend_never ShaderModuleManager::ShaderOutputView::final_suspend() noexcept {
	return { };
}

suspend_always ShaderModuleManager::ShaderOutputView::yield_value(const ShaderOutputView shader_output) noexcept {
	*this = shader_output;
	return { };
}

void ShaderModuleManager::ShaderOutputView::unhandled_exception() {
	try {
		std::rethrow_exception(std::current_exception());
	} catch (const std::exception& e) {
		std::cerr << e.what() << endl;
		std::terminate();
	}
}

shaderc::CompileOptions ShaderModuleManager::createCommonShaderCompileOption() {
	shaderc::CompileOptions option;
	//compiler optimisation
#ifndef NDEBUG
	option.SetGenerateDebugInfo();
	option.SetOptimizationLevel(shaderc_optimization_level_zero);
#else
	option.SetOptimizationLevel(shaderc_optimization_level_performance);
#endif

	//language standard
	option.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
	option.SetTargetSpirv(shaderc_spirv_version_1_6);

	//include
	option.SetIncluder(make_unique<ShaderIncluder>());

	return option;
}

void ShaderModuleManager::_Internal::batchShaderCompilation(const ShaderBatchCompilationInfo& info, ostream& out,
	const shaderc::CompileOptions& option, const span<ShaderOutput> shader_out) {
	const auto [device, shader_filename, shader_kind] = info;
	shaderc::Compiler compiler;

	for (size_t i = 0u; i < shader_out.size(); i++) {
		const char* const current_filename = shader_filename[i].data();
		const shaderc_shader_kind current_kind = shader_kind[i];

		const shaderc::CompilationResult result = compiler.CompileGlslToSpv(
			File::readString(current_filename), current_kind, current_filename, option);

		ShaderOutput& current_out = shader_out[i];
		current_out.Code = processShaderCompilationResult(result, out);
		const StaticArray<uint32_t>& bin = current_out.Code;
		current_out.SMInfo = {
			.sType = VkStructureType::VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.codeSize = bin.size() * sizeof(uint32_t),
			.pCode = bin.data()
		};
		current_out.Stage = ::fromKindToStage(current_kind);
	}
}