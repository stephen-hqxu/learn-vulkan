#include "Camera.hpp"

#include "Abstraction/BufferManager.hpp"
#include "../Common/ErrorHandler.hpp"
#include "../Common/FixedArray.hpp"

#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>

#include <glm/trigonometric.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <ranges>
#include <stdexcept>

#include <cassert>

using glm::mat4;
using glm::vec3, glm::dvec2, glm::dvec3, glm::dmat4;
using glm::lookAt, glm::perspective, glm::normalize, glm::radians;

using std::ranges::generate, std::ranges::for_each, std::ranges::transform, std::ranges::fill;
using std::make_pair;
using std::array;

using namespace LearnVulkan;
namespace VKO = VulkanObject;

struct Camera::PackedCameraBuffer {

	mat4 V, PV, InvPVRot;
	vec3 Pos;

	float _pad0;
	vec3 LDF;

};

namespace {

	inline VKO::DescriptorSetLayout createCameraDescriptorSetLayout(const VkDevice device) {
		constexpr static VkDescriptorSetLayoutBinding dslb_info {
			.binding = 0u,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1u,
			.stageFlags = VK_SHADER_STAGE_ALL
		};
		constexpr static VkDescriptorSetLayoutCreateInfo dsl_info {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
			.bindingCount = 1u,
			.pBindings = &dslb_info
		};
		return VKO::createDescriptorSetLayout(device, dsl_info);
	}

	inline constexpr void setTrue(bool& b) noexcept {
		b = true;
	}

}

Camera::Camera(const CreateInfo& camera_create_info) : CameraInfo(*camera_create_info.CameraInfo), Dirty { } {
	this->updateViewSpace();
	const VulkanContext& ctx = *camera_create_info.Context;
	
	const BufferManager::BufferCreateInfo buf_info = { ctx.Device, ctx.Allocator, sizeof(PackedCameraBuffer) };
	generate(this->ShaderBuffer, [&buf_info = std::as_const(buf_info), &cam_info = camera_create_info.CameraInfo]() {
		VKO::BufferAllocation camera_buf = BufferManager::createGlobalStorageBuffer(buf_info, BufferManager::HostAccessPattern::Sequential);
		auto mapped = VKO::mapAllocation<PackedCameraBuffer>(buf_info.Allocator, camera_buf.first);

		*mapped = { };
		const double near = cam_info->Near,
			far = cam_info->Far;
		mapped->LDF = vec3(
			far * near,
			far - near,
			far
		);
		CHECK_VULKAN_ERROR(vmaFlushAllocation(buf_info.Allocator, camera_buf.first, 0ull, VK_WHOLE_SIZE));

		return make_pair(std::move(camera_buf), std::move(mapped));
	});

	/***************************
	 * Create descriptor buffer
	 ***************************/
	this->DescriptorSetLayout = ::createCameraDescriptorSetLayout(this->getDevice());

	//we need to allocate a descriptor set for each in-flight frame
	array<VkDescriptorSetLayout, EngineSetting::MaxFrameInFlight> camera_ds_layout;
	fill(camera_ds_layout, *this->DescriptorSetLayout);
	this->DescriptorBuffer = DescriptorBufferManager(ctx, camera_ds_layout, VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT);

	/***************************
	 * Update descriptor buffer
	 **************************/
	using DU = DescriptorBufferManager::DescriptorUpdater;
	const DU camera_ds_updater = this->DescriptorBuffer.createUpdater(ctx);
	VkDescriptorAddressInfoEXT addr_info {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT,
		.range = sizeof(PackedCameraBuffer),
	};
	DU::UpdateInfo update_info {
		.SetLayout = this->DescriptorSetLayout,
		.Binding = 0u,
		.GetInfo = {
			.Type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.Data = { .pStorageBuffer = &addr_info }
		}
	};

	for (const auto i : std::views::iota(0u, this->ShaderBuffer.size())) {
		addr_info.address = BufferManager::addressOf(this->getDevice(), this->ShaderBuffer[i].first.second);
		update_info.SetIndex = i;
		camera_ds_updater.update(update_info);
	}
}

void Camera::updateViewSpace() noexcept {
	const double cos_pitch = glm::cos(this->CameraInfo.Pitch);
	this->Front = normalize(dvec3(
		glm::cos(this->CameraInfo.Yaw) * cos_pitch,
		glm::sin(this->CameraInfo.Pitch),
		glm::sin(this->CameraInfo.Yaw) * cos_pitch
	));
	this->Right = normalize(cross(this->Front, this->CameraInfo.WorldUp));
	this->Up = normalize(cross(this->Right, this->Front));
}

VkDevice Camera::getDevice() const noexcept {
	//all buffers uses the same logical device, pick any
	return this->ShaderBuffer[0].first.second->get_deleter().Device;
}

void Camera::dirtyProjection() noexcept {
	for_each(this->Dirty, setTrue, [](DirtyFlag& dirty) constexpr noexcept -> bool& { return dirty.Projection; });
}

void Camera::dirtyView() noexcept {
	for_each(this->Dirty, setTrue, [](DirtyFlag& dirty) constexpr noexcept -> bool& { return dirty.View; });
}

void Camera::dirtyPosition() noexcept {
	for_each(this->Dirty, setTrue, [](DirtyFlag& dirty) constexpr noexcept -> bool& { return dirty.Position; });
}

VkDescriptorSetLayout Camera::descriptorSetLayout() const noexcept {
	return this->DescriptorSetLayout;
}

VkDescriptorBufferBindingInfoEXT Camera::descriptorBufferBindingInfo() const noexcept {
	return {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
		.address = BufferManager::addressOf(this->getDevice(), this->DescriptorBuffer.buffer()),
		.usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT
	};
}

VkDeviceSize Camera::descriptorBufferOffset(const unsigned int index) const noexcept {
	return this->DescriptorBuffer.offset(index);
}

void Camera::update(const unsigned int index) {
	DirtyFlag& dirty = this->Dirty[index];
	const auto& [buffer, camera_memory] = this->ShaderBuffer[index];
	const CameraData& ci = this->CameraInfo;

	constexpr static size_t FlushCount = 4u;
	FixedArray<VkDeviceSize, FlushCount> offset, size;

#define PUSH_OFFSET_SIZE(FIELD) \
offset.pushBack(offsetof(PackedCameraBuffer, FIELD)); \
size.pushBack(sizeof(PackedCameraBuffer::FIELD))

	dmat4 view, projection;
	if (dirty.Projection || dirty.View) {
		view = lookAt(ci.Position, ci.Position + this->Front, this->Up);
		projection = perspective(ci.FieldOfView, ci.Aspect, ci.Far, ci.Near);
		
		const dmat4 inv_projection = glm::inverse(projection),
			view_rotation = dmat4(glm::dmat3(view)),
			inv_view_rotation = glm::transpose(view_rotation);

		camera_memory->PV = projection * view;
		camera_memory->InvPVRot = inv_view_rotation;

		PUSH_OFFSET_SIZE(PV);
		PUSH_OFFSET_SIZE(InvPVRot);
	}
	if (dirty.View) {
		camera_memory->V = view;

		PUSH_OFFSET_SIZE(V);
	}
	if (dirty.Position) {
		camera_memory->Pos = ci.Position;

		PUSH_OFFSET_SIZE(Pos);
	}

#undef PUSH_OFFSET_SIZE

	assert(offset.size() == size.size());
	if (const bool require_flush = offset.size() > 0u;
		!require_flush) {
		return;
	}

	array<VmaAllocation, FlushCount> allocation;
	fill(allocation | std::views::take(offset.size()), *buffer.first);
	//more efficient to do a batched flush
	CHECK_VULKAN_ERROR(vmaFlushAllocations(buffer.first->get_deleter().Allocator, static_cast<uint32_t>(offset.size()),
		allocation.data(), offset.data(), size.data()));

	dirty = { };
}

void Camera::move(const MoveDirection direction, const double delta) {
	const double velocity = this->CameraInfo.MovementSpeed * delta;

	using enum MoveDirection;
	switch (direction) {
	case Forward: this->CameraInfo.Position += this->Front * velocity;
		break;
	case Backward: this->CameraInfo.Position -= this->Front * velocity;
		break;
	case Left: this->CameraInfo.Position -= this->Right * velocity;
		break;
	case Right: this->CameraInfo.Position += this->Right * velocity;
		break;
	case Up: this->CameraInfo.Position += this->CameraInfo.WorldUp * velocity;
		break;
	case Down: this->CameraInfo.Position -= this->CameraInfo.WorldUp * velocity;
		break;
	default:
		throw std::runtime_error("Unknown camera movement direction enum.");
	}
	this->dirtyView();
	this->dirtyPosition();
}

void Camera::rotate(const dvec2& offset) noexcept {
	constexpr static double YawMax = radians(360.0), PitchMax = radians(89.0);

	const dvec2 rotateAmount = offset * this->CameraInfo.RotationSpeed;
	this->CameraInfo.Yaw += rotateAmount.x;
	this->CameraInfo.Yaw = glm::mod(this->CameraInfo.Yaw, YawMax);
	this->CameraInfo.Pitch += rotateAmount.y;
	this->CameraInfo.Pitch = glm::clamp(this->CameraInfo.Pitch, -PitchMax, PitchMax);

	this->updateViewSpace();
	this->dirtyView();
}

void Camera::setAspect(const double width, const double height) noexcept {
	this->CameraInfo.Aspect = width / height;
	this->dirtyProjection();
}