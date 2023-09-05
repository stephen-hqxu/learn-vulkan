#include "Common/File.hpp"
#include "Common/ErrorHandler.hpp"
#include "Common/VulkanObject.hpp"

#include "Engine/Camera.hpp"
#include "Engine/EngineSetting.hpp"
#include "Engine/MasterEngine.hpp"
#include "Engine/VulkanContext.hpp"
#include "Engine/Abstraction/ImageManager.hpp"

#include "Renderer/DrawTriangle.hpp"
#include "Renderer/SimpleTerrain.hpp"

//Rendering Framework
#include <Volk/volk.h>
#include <GLFW/glfw3.h>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/trigonometric.hpp>

#include <memory>
#include <source_location>
#include <stdexcept>
#include <iostream>

#include <array>
#include <string_view>

using glm::dvec2, glm::dvec3;
using glm::radians;

using std::unique_ptr, std::make_unique;
using std::source_location;
using std::runtime_error;
using std::cout, std::cerr, std::endl;

using std::string_view;

namespace VKO = LearnVulkan::VulkanObject;

namespace {

	constexpr double MinFrameTime = 1.0 / 65.5;/**< The minimum amount of time between two frames, in seconds. */

	constexpr unsigned int InitialWidth = 720u, InitialHeight = 720u;
	constexpr LearnVulkan::Camera::CameraData CameraData = {
		.Yaw = radians(-90.0),
		.Pitch = radians(-30.0),
		.FieldOfView = radians(60.5),
		.MovementSpeed = 25.5,
		.RotationSpeed = 0.0005,
		.Position = dvec3(0.0, 1.0, 3.0),
		.WorldUp = dvec3(0.0, 1.0, 0.0),
		.Aspect = (1.0 * InitialWidth) / (1.0 * InitialHeight),
		.Near = 0.8,
		.Far = 1155.5
	};

	/**
	 * @brief The name of each sample application.
	*/
	enum class SampleApplicationName : uint8_t {
		Triangle = 0x00u,
		Terrain = 0x10u,
		Water = 0x11u,
		Invalid = 0xFFu
	};

	/////////////////////////////////////////////////////////////////////////////
	///								Event Handling
	////////////////////////////////////////////////////////////////////////////

	//Record the status of event processing of drawing canvas.
	struct CanvasEventStatus {

		bool NeedReshape, CursorMoved;

	};

	void checkGLFWError(const int err_code, const source_location src = source_location::current()) {
		if (err_code != GLFW_TRUE) {
			LearnVulkan::ErrorHandler::throwError("GLFW has encountered an error!", src);
		}
	}
#define CHECK_GLFW_ERROR(FUNC) checkGLFWError(FUNC)

	void resizeCanvas(GLFWwindow* const canvas, int, int) {
		reinterpret_cast<CanvasEventStatus*>(glfwGetWindowUserPointer(canvas))->NeedReshape = true;
	}

	void moveCursor(GLFWwindow* const canvas, double, double) {
		reinterpret_cast<CanvasEventStatus*>(glfwGetWindowUserPointer(canvas))->CursorMoved = true;
	}

	void processKeystroke(GLFWwindow* const canvas, LearnVulkan::Camera& camera, const double delta) noexcept {
		using enum LearnVulkan::Camera::MoveDirection;
		if (glfwGetKey(canvas, GLFW_KEY_W)) {
			camera.move(Forward, delta);
		}
		if (glfwGetKey(canvas, GLFW_KEY_S)) {
			camera.move(Backward, delta);
		}
		if (glfwGetKey(canvas, GLFW_KEY_A)) {
			camera.move(Left, delta);
		}
		if (glfwGetKey(canvas, GLFW_KEY_D)) {
			camera.move(Right, delta);
		}
		if (glfwGetKey(canvas, GLFW_KEY_SPACE)) {
			camera.move(Up, delta);
		}
		if (glfwGetKey(canvas, GLFW_KEY_C)) {
			camera.move(Down, delta);
		}

		if (glfwGetKey(canvas, GLFW_KEY_ESCAPE)) {
			glfwSetWindowShouldClose(canvas, GLFW_TRUE);
		}
	}

	struct CanvasDestroyer {
	public:

		inline void operator()(GLFWwindow* const canvas) const noexcept {
			glfwDestroyWindow(canvas);
		}

	};
	using CanvasHandle = unique_ptr<GLFWwindow, CanvasDestroyer>;/**< GLFWwindow */

	////////////////////////////////////////////////////////////////////
	///						Context Initialisation
	////////////////////////////////////////////////////////////////////
	/**
	 * @brief Initialise the drawing canvas.
	 * @return Canvas.
	*/
	CanvasHandle initCanvas() {
		/*******************************
		 * Canvas initialisation setup
		 *******************************/
		CHECK_GLFW_ERROR(glfwVulkanSupported());

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		GLFWwindow* const canvas = glfwCreateWindow(InitialWidth, InitialHeight, "Vulkan Tutorial", nullptr, nullptr);
		if (!canvas) {
			throw runtime_error("Unable to initialise GLFW window");
		}
		auto canvas_handle = CanvasHandle(canvas);
		
		/*************************************
		 * Post-initialisation configuration
		 *************************************/
		glfwSetFramebufferSizeCallback(canvas, &resizeCanvas);
		glfwSetCursorPosCallback(canvas, &moveCursor);
		glfwSetInputMode(canvas, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

		return canvas_handle;
	}

	void runApplication(const SampleApplicationName app_name) {
		const CanvasHandle canvas_handle = initCanvas();
		GLFWwindow* const canvas = canvas_handle.get();

		LearnVulkan::MasterEngine engine(canvas, CameraData, cout);

		//Create sample application based on selection of app_name.
		const auto createSampleApplication = [&engine, app_name]() -> unique_ptr<LearnVulkan::RendererInterface> {
			using std::array;

			using namespace LearnVulkan;
			namespace IM = ImageManager;
			using enum SampleApplicationName;
			using EngineSetting::ResourceRoot;

			const VulkanContext& ctx = engine.context();
			switch (app_name) {
			case Triangle:
			{
				constexpr static string_view TriangleImageFilename = "/WoodFloor051_1K-PNG/WoodFloor051_1K_Color.png";
				constexpr static auto TriangleImageFullPath = File::toAbsolutePath<ResourceRoot, TriangleImageFilename>();
				constexpr static array TriangleImageFullPathArray = { TriangleImageFullPath.data() };

				constexpr static IM::ImageReadInfo triangle_image_info {
					.Channel = 4,
					.ColourSpace = IM::ImageColourSpace::SRGB
				};
				const IM::ImageReadResult triangle_image = IM::readFile<IM::ImageBitWidth::Eight>(ctx.Device, ctx.Allocator,
					TriangleImageFullPathArray, triangle_image_info);

				const DrawTriangle::TriangleCreateInfo triangle_info {
					.CameraDescriptorSetLayout = engine.camera().descriptorSetLayout(),
					.SurfaceTexture = &triangle_image,
					.DebugMessage = &cout
				};
				return make_unique<DrawTriangle>(ctx, triangle_info);
			}
			//both terrain and water sample uses the same renderer
			//in terrain renderer's case, we configure the terrain renderer to exclude water 
			case Terrain:
			case Water:
			{
				const bool draw_water = app_name == Water;

				constexpr static string_view TerrainHeightmapFilename = "/Heightfield-Texture-Sample/heightfield.png",
					TerrainNormalmapFilename = "/Heightfield-Texture-Sample/heightfield_normal.png",
					WaterNormalmapFilename = "/waterNormal.png", WaterDistortionFilename = "/waterDUDV.png";
				constexpr static auto TerrainHeightmapFullPath = File::toAbsolutePath<ResourceRoot, TerrainHeightmapFilename>();
				constexpr static auto TerrainNormalmapFullPath = File::toAbsolutePath<ResourceRoot, TerrainNormalmapFilename>();
				constexpr static auto WaterNormalmapFullPath = File::toAbsolutePath<ResourceRoot, WaterNormalmapFilename>();
				constexpr static auto WaterDistortionFullPath = File::toAbsolutePath<ResourceRoot, WaterDistortionFilename>();
				constexpr static array TerrainHeightmapFullPathArray = { TerrainHeightmapFullPath.data() },
					TerrainNormalmapFullPathArray = { TerrainNormalmapFullPath.data() },
					WaterNormalmapFullPathArray = { WaterNormalmapFullPath.data() },
					WaterDistortionFullPathArray = { WaterDistortionFullPath.data() };

				constexpr static IM::ImageReadInfo heightmap_info {
					.Channel = 1,
					.ColourSpace = IM::ImageColourSpace::Linear
				}, normalmap_info {
					.Channel = 4,
					.ColourSpace = IM::ImageColourSpace::Linear
				}, water_normalmap_info {
					.Channel = 4,
					.ColourSpace = IM::ImageColourSpace::Linear
				}, water_distortion_info {
					.Channel = 2,
					.ColourSpace = IM::ImageColourSpace::Linear
				};
				const IM::ImageReadResult heightmap = IM::readFile<IM::ImageBitWidth::Sixteen>(ctx.Device, ctx.Allocator,
						TerrainHeightmapFullPathArray, heightmap_info),
					normalmap = IM::readFile<IM::ImageBitWidth::Sixteen>(ctx.Device, ctx.Allocator,
						TerrainNormalmapFullPathArray, normalmap_info);
				
				IM::ImageReadResult water_normalmap, water_distortion;
				SimpleTerrain::TerrainWaterCreateInfo terrain_water_info;
				if (draw_water) {
					water_normalmap = IM::readFile<IM::ImageBitWidth::Eight>(ctx.Device, ctx.Allocator,
						WaterNormalmapFullPathArray, water_normalmap_info);
					water_distortion = IM::readFile<IM::ImageBitWidth::Eight>(ctx.Device, ctx.Allocator,
						WaterDistortionFullPathArray, water_distortion_info);

					terrain_water_info = {
						.WaterNormalmap = &water_normalmap,
						.WaterDistortion = &water_distortion
					};
				}

				const SimpleTerrain::TerrainCreateInfo terrain_info {
					.CameraDescriptorSetLayout = engine.camera().descriptorSetLayout(),
					.WaterInfo = draw_water ? &terrain_water_info : nullptr,
					.Heightmap = &heightmap,
					.Normalmap = &normalmap,
					.DebugMessage = &cout
				};
				return make_unique<SimpleTerrain>(ctx, terrain_info);
			}
			default: throw runtime_error("The sample application name specified is unknown");
			}
		};
		//initialise renderer and create framebuffer for swap chain using the render pass from renderer
		const auto renderer = createSampleApplication();
		engine.attachRenderer(renderer.get());

		const auto clean_up = [canvas, &engine]() -> void {
			CHECK_VULKAN_ERROR(vkDeviceWaitIdle(engine.context().Device));
			engine.attachRenderer(nullptr);
		};
		CanvasEventStatus canvas_event;
		dvec2 last_cursor_position;
		{
			//load up initial cursor position
			double x, y;
			glfwGetCursorPos(canvas, &x, &y);
			last_cursor_position = dvec2(x, y);
		}
		double last_time = glfwGetTime();
		glfwSetWindowUserPointer(canvas, &canvas_event);
		while (!glfwWindowShouldClose(canvas)) {
			//frame time limit logic
			double delta_time;
			{
				double current_time;
				do {
					current_time = glfwGetTime();
					delta_time = current_time - last_time;
				} while (delta_time < MinFrameTime);
				last_time = current_time;
			}

			//I/O event
			glfwPollEvents();
			processKeystroke(canvas, engine.camera(), delta_time);
			if (canvas_event.NeedReshape) {
				//camera is automatically reshaped when the engine context is reshaped
				engine.reshape(canvas);
				canvas_event.NeedReshape = false;
			}
			if (canvas_event.CursorMoved) {
				double x, y;
				glfwGetCursorPos(canvas, &x, &y);
				const dvec2 current_pos = dvec2(x, y),
					offset = dvec2(current_pos.x - last_cursor_position.x, last_cursor_position.y - current_pos.y);
				last_cursor_position = current_pos;

				engine.camera().rotate(offset);
				canvas_event.CursorMoved = false;
			}

			try {
				engine.draw(delta_time);
			} catch (...) {
				clean_up();
				throw;
			}
		}
		clean_up();
	}

}

int main(const int argc, const char* const* const argv) {
	cout << "Learn Vulkan Demo Suite" << endl;
	if (argc == 1) {
		cout << "Please specify which sample to run:\n";
		cout << "Available options:\n";
		cout << "-> triangle\n";
		cout << "-> terrain\n";
		cout << "-> water" << endl;
		return EXIT_SUCCESS;
	}

	using enum SampleApplicationName;
	SampleApplicationName app_name = Invalid;
	if (const string_view selection = argv[1];
		selection == "triangle") {
		app_name = Triangle;
		cout << "My very first Vulkan application, demonstrates the basic workflow to setup a Vulkan renderer." << endl;
	} else if (selection == "terrain") {
		app_name = Terrain;
		cout << "Demonstration of implementing a terrain renderer using compute and tessellation shader." << endl;
	} else if (selection == "water") {
		app_name = Water;
		cout << "First experience of diving into ray tracing to render reflective and refractive water." << endl;
	} else {
		cout << "Unknown sample name \'" << selection << '\'' << endl;
		return EXIT_SUCCESS;
	}

	try {
		CHECK_GLFW_ERROR(glfwInit());
		CHECK_VULKAN_ERROR(volkInitialize());
		runApplication(app_name);
		glfwTerminate();
	} catch (const std::exception& e) {
		cerr << e.what() << endl;
		glfwTerminate();
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}