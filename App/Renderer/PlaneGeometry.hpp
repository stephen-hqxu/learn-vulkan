#pragma once

#include "GeometryData.hpp"

#include "../Engine/VulkanContext.hpp"
#include "../Common/VulkanObject.hpp"
#include "../Common/FixedArray.hpp"

#include <glm/vec2.hpp>

#include <optional>
#include <ostream>
#include <cstdint>

namespace LearnVulkan {

	/**
	 * @brief 2D plane geometry procedural generator using compute shader.
	 * The generated plane will have its top-left corner as world origin.
	*/
	class PlaneGeometry {
	public:

		/**
		 * @brief Specify the properties of the generated plane.
		*/
		struct Property {

			glm::dvec2 Dimension;
			glm::uvec2 Subdivision;
			//Specify that if the generated geometry data will be used for building a GAS.
			bool RequireAccelStructInput;

		};

		/**
		 * @brief Used to provide displacement information of the plane geometry.
		*/
		struct Displacement {

			float Altitude;
			VkDescriptorImageInfo DisplacementMap;

		};

	private:

		const struct {
		
			VulkanObject::DescriptorSetLayout PlaneProperty, DisplacementMap;

		} DescriptorSet;
		const struct {

			VulkanObject::PipelineLayout Generator, Displacer;

		} PipelineLayout;
		struct {
			
			VulkanObject::Pipeline Generator, Displacer;
		
		} Pipeline;

		/**
		 * @brief Initialise a geometry data, including making certain memory allocation, for later geometry generation.
		 * @param ctx The context.
		 * @param prop The plane property.
		 * @param geo The geometry data to be initialised.
		 * @return The command buffer within the geometry data for which subsequent commands after initialisation
		 * can be recorded to. Command buffer has been begun and requires manual ending.
		*/
		VkCommandBuffer prepareGeometryData(const VulkanContext&, const Property&, GeometryData&) const;

		/**
		 * @brief Bind descriptor buffer for a geometry data.
		*/
		static void bindDescriptorBuffer(VkDevice, VkCommandBuffer, VkPipelineLayout, const GeometryData&) noexcept;

		/**
		 * @brief Dispatch compute on a geometry data.
		 * The last argument specify the number workgroup in Z-axis.
		*/
		static void dispatch(VkCommandBuffer, const GeometryData&, uint32_t);

	public:

		constexpr static VkPipelineStageFlagBits2 DisplacementStage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
		constexpr static VkAccessFlagBits2 DisplacementAccess = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;

		/**
		 * @brief Provide user-specified parameters to vertex input data.
		*/
		struct VertexInputCustomisation {

			uint32_t BindingIndex;
			struct {
				
				//If ignored, the corresponded location will be omitted.
				std::optional<uint32_t> Position, UV;
			
			} Location;

		};

		/**
		 * @brief The vertex input information for graphics pipeline regarding the plane geometry.
		*/
		struct VertexInput {

			VkVertexInputBindingDescription Binding;
			FixedArray<VkVertexInputAttributeDescription, 2u> Attribute;

		};

		/**
		 * @brief Initial the plane geometry generator.
		 * @param ctx The vulkan context.
		 * @param msg A stream to receive diagnostic messages.
		*/
		PlaneGeometry(const VulkanContext&, std::ostream&);

		PlaneGeometry(const PlaneGeometry&) = delete;

		PlaneGeometry(PlaneGeometry&&) = delete;

		PlaneGeometry& operator=(const PlaneGeometry&) = delete;

		PlaneGeometry& operator=(PlaneGeometry&&) = delete;

		~PlaneGeometry() = default;

		/**
		 * @brief Get the vertex input information.
		 * @param customisation Provide user parameters.
		 * @return The vertex input information.
		*/
		static VertexInput vertexInput(const VertexInputCustomisation&) noexcept;

		/**
		 * @brief Initiate plane generation.
		 * The generation is asynchronous. Thus pipeline barrier is required if buffer will be used by subsequent commands.
		 * @param ctx The context.
		 * @param prop The plane property.
		 * @param geo The output where geometry data will be stored.
		 * All old data will be destroyed. The behaviour is undefined if
		 * the geometry data is still being used a previously unfinished generate command.
		 * @return The generation command buffer, which is owned by geometry data, and which is a secondary command buffer.
		*/
		VkCommandBuffer generate(const VulkanContext&, const Property&, GeometryData&) const;

		/**
		 * @brief Displace each vertex in the plane geometry in vertical direction based on a displacement map.
		 * @param ctx The context.
		 * @param disp The displacement info.
		 * @param geo The geometry where displacement will be applied.
		 * The geometry must be a valid plane geometry.
		 * @return The displacement command buffer, owned by geometry data.
		 * @exception If the geometry data is not a valid plane geometry.
		*/
		VkCommandBuffer displace(const VulkanContext&, const Displacement&, GeometryData&) const;

	};

}