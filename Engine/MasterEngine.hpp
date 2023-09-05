#pragma once

#include "../Common/VulkanObject.hpp"
#include "../Common/StaticArray.hpp"

#include "Camera.hpp"
#include "EngineSetting.hpp"
#include "RendererInterface.hpp"

#include "ContextManager.hpp"
#include "VulkanContext.hpp"

#include <GLFW/glfw3.h>

#include <ostream>
#include <array>
#include <optional>
#include <memory>

#include <cstdint>

namespace LearnVulkan {

	/**
	 * @brief The master engine that drives all renderers.
	*/
	class MasterEngine {
	private:

		struct DrawSynchronisationPrimitive {

			VulkanObject::Semaphore ImageAvailable, RenderFinish, WaitFrame;
			mutable uint64_t FrameCounter;

		};

		/**
		 * @brief Opaque user data used for debug callback.
		*/
		struct DebugCallbackUserData;
		const std::unique_ptr<DebugCallbackUserData> DbgCbUserData;

		//context
		VulkanContext Context;
		VulkanObject::SurfaceKHR Surface;
		VulkanObject::DebugUtilsMessengerEXT DebugMessage;

		//presentation
		VulkanObject::SwapchainKHR SwapChain;
		ContextManager::SwapchainImage SwapChainImage;
		VkExtent2D SwapChainExtent;

		//rendering
		std::array<DrawSynchronisationPrimitive, EngineSetting::MaxFrameInFlight> DrawSync;

		//our objects
		mutable std::optional<Camera> SceneCamera;
		RendererInterface* AttachedRenderer;

		/**
		 * @brief Allow rendering the next frame before the previous one has finished.
		 * This counter keeps track of which sub-frame we are working on right now.
		 * @see EngineSetting::MaxFrameInFlight
		*/
		mutable unsigned int FrameInFlightIndex;

		/**
		 * @brief Create (or recreate if already) presentation context.
		 * @param canvas The canvas to be presented on.
		*/
		void createPresentation(GLFWwindow*);

	public:

		/**
		 * @brief Initialise the master engine.
		 * The caller should retain the lifetime of all references until the constructing master engine is destroyed.
		 * @param canvas Specify the canvas used for drawing.
		 * @param camera_data The intrinsic properties to create a camera.
		 * @param msg A stream to be used for debug message output.
		*/
		MasterEngine(GLFWwindow*, const Camera::CameraData&, std::ostream&);

		MasterEngine(const MasterEngine&) = delete;

		MasterEngine(MasterEngine&&) = delete;

		MasterEngine& operator=(const MasterEngine&) = delete;

		MasterEngine& operator=(MasterEngine&&) = delete;

		~MasterEngine();

		///////////////////////////////////////
		///				Getter
		//////////////////////////////////////
		const VulkanContext& context() const noexcept;
		Camera& camera() noexcept;
		//////////////////////////////////////

		/**
		 * @brief Attach a renderer to the master engine, and create a framebuffer using with renderer.
		 * @param renderer The renderer to be attached.
		 * Can be a null pointer to detach the current renderer.
		 * If pointer is not null, its lifetime must remain until it is detached.
		*/
		void attachRenderer(RendererInterface*);

		/**
		 * @brief Reshape the presentation context.
		 * This function should be called whenever the canvas has reshaped.
		 * @param canvas The canvas to be reshaped.
		*/
		void reshape(GLFWwindow*);

		/**
		 * @brief Execute draw command on one frame.
		 * @param delta_time The time since the last frame.
		*/
		void draw(double) const;

	};

}