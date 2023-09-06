#pragma once

#include "CameraInterface.hpp"

#include "../Common/VulkanObject.hpp"
#include "Abstraction/DescriptorBufferManager.hpp"
#include "EngineSetting.hpp"
#include "VulkanContext.hpp"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <array>
#include <utility>

namespace LearnVulkan {

	/**
	 * @brief Camera utility for 3D rendering.
	*/
	class Camera : public CameraInterface {
	public:

		/**
		 * @brief Specifies a collection of possible movement directions of the camera.
		*/
		enum class MoveDirection : unsigned char {
			Forward = 0x00u,
			Backward = 0x01u,
			Left = 0x10u,
			Right = 0x11u,
			Up = 0x20u,
			Down = 0x21u
		};

		/**
		 * @brief Intrinsic parameters of the camera.
		 * All angles are specified in radians.
		*/
		struct CameraData {

			double Yaw, Pitch;
			double FieldOfView;
			double MovementSpeed, RotationSpeed;

			glm::dvec3 Position, WorldUp;

			double Aspect;
			double Near, Far;

		};

		/**
		 * @brief Creation information for the camera class.
		*/
		struct CreateInfo {

			const VulkanContext* Context;
			const CameraData* CameraInfo;

		};

	private:

		/**
		 * @see Shader/CameraData.glsl.
		*/
		struct PackedCameraBuffer;

		//camera intrinsic data
		CameraData CameraInfo;
		glm::dvec3 Front, Up, Right;

		/**
		 * @brief Status flags to record which matrix needs to be updated.
		*/
		struct DirtyFlag {

			bool Projection, View, Position;

		};
		std::array<DirtyFlag, EngineSetting::MaxFrameInFlight> Dirty;/**< For each concurrent frame. */

		//camera shader memory
		std::array<std::pair<VulkanObject::BufferAllocation, VulkanObject::MappedAllocation<PackedCameraBuffer>>,
			EngineSetting::MaxFrameInFlight> ShaderBuffer;

		VulkanObject::DescriptorSetLayout DescriptorSetLayout;
		DescriptorBufferManager DescriptorBuffer;

		/**
		 * @brief Update the camera vectors.
		*/
		void updateViewSpace() noexcept;

		/**
		 * @brief Get the logical device.
		 * @return The logical device.
		*/
		VkDevice getDevice() const noexcept;

		//Mark the respective field dirty.
		void dirtyProjection() noexcept;
		void dirtyView() noexcept;
		void dirtyPosition() noexcept;

	public:

		/**
		 * @brief Initialise a camera.
		 * @param camera_create_info Information to create a new camera.
		*/
		Camera(const CreateInfo&);

		Camera(const Camera&) = delete;

		Camera(Camera&&) noexcept = default;

		Camera& operator=(const Camera&) = delete;

		Camera& operator=(Camera&&) noexcept = default;

		~Camera() override = default;

		VkDescriptorSetLayout descriptorSetLayout() const noexcept override;

		VkDescriptorBufferBindingInfoEXT descriptorBufferBindingInfo() const noexcept override;

		VkDeviceSize descriptorBufferOffset(unsigned int) const noexcept override;

		/**
		 * @brief Re-compute the internal camera matrix and flush the camera uniform buffer after the internal state has been updated.
		 * @param index The index of in-flight frame to be updated.
		*/
		void update(unsigned int);

		/**
		 * @brief Move position of the camera in the world.
		 * @param direction Specify in which direction the camera should be moved to.
		 * @param delta A multiplier to the movement speed.
		*/
		void move(MoveDirection, double);

		/**
		 * @brief Rotate the orientation of the camera in the world.
		 * @param offset The relative angle of offset of the camera.
		*/
		void rotate(const glm::dvec2&) noexcept;

		/**
		 * @brief Set the aspect ratio of the view frustum.
		 * @param width The new frustum width.
		 * @param height The new frustum height.
		*/
		void setAspect(double, double) noexcept;

	};

}