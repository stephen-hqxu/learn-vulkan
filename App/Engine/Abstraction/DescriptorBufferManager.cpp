#include "DescriptorBufferManager.hpp"

#include "../../Common/ErrorHandler.hpp"
#include "BufferManager.hpp"

#include <algorithm>
#include <numeric>
#include <ranges>
#include <bit>

#include <stdexcept>
#include <cassert>

using std::span;
using std::runtime_error;

using namespace LearnVulkan;
namespace VKO = VulkanObject;

namespace {

	inline constexpr VkDescriptorGetInfoEXT toDescriptorGetInfo(const DescriptorBufferManager::DescriptorUpdater::DescriptorGetInfo& ds) noexcept {
		const auto [type, data] = ds;
		return {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
			.type = type,
			.data = data
		};
	}

	size_t getDataSize(const VulkanContext& ctx, const VkDescriptorGetInfoEXT& ds) {
		const VkPhysicalDeviceDescriptorBufferPropertiesEXT& prop = ctx.PhysicalDeviceProperty.DescriptorBuffer;
		switch (ds.type) {
		case VK_DESCRIPTOR_TYPE_SAMPLER:
			return prop.samplerDescriptorSize;
		case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			return prop.combinedImageSamplerDescriptorSize;
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			return prop.uniformBufferDescriptorSize;
		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
			return prop.storageBufferDescriptorSize;
		case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
			return prop.accelerationStructureDescriptorSize;
		default:
			throw runtime_error("The type of descriptor buffer is unsupported.");
		}
	}

}

DescriptorBufferManager::DescriptorUpdater::DescriptorUpdater(const VulkanContext& ctx, DescriptorBufferManager& dbm) :
	Context(&ctx), DesBufManager(&dbm) {
	if (dbm.UpdaterAlive) {
		throw runtime_error("It's only allowed to have one descriptor update alive at a time.");
	}
	dbm.clearFlush();
	dbm.UpdaterAlive = true;

	//we are using a dummy type for pointer, because unique_ptr doesn't allow void*
	this->Mapped = VKO::mapAllocation<uint8_t>(ctx.Allocator, dbm.DescriptorBuffer.first);
	this->Flusher = { this->Mapped.get(), { this } };
}

void DescriptorBufferManager::DescriptorUpdater::DescriptorFlusher::operator()(uint8_t*) const {
	auto& dmb = *this->Updater->DesBufManager;
	const auto& [allocation, offset, size] = dmb.Flush;
	
	const size_t update_count = allocation.size();
	assert(offset.size() == update_count && size.size() == update_count);

	CHECK_VULKAN_ERROR(vmaFlushAllocations(this->Updater->Context->Allocator, static_cast<uint32_t>(update_count),
		allocation.data(), offset.data(), size.data()));
	dmb.UpdaterAlive = false;
}

void DescriptorBufferManager::DescriptorUpdater::update(const UpdateInfo& update_info) const {
	const auto& [ds_layout, set_idx, binding, layer, get_info] = update_info;
	auto& dmb = *this->DesBufManager;
	const auto& ctx = *this->Context;
	assert(dmb.UpdaterAlive);

	//compute set offset
	const VkDeviceSize set_offset = dmb.Offset[set_idx];
	uint8_t* const buf_ptr = this->Mapped.get();

	//compute binding offset
	const VkDescriptorGetInfoEXT vk_get = ::toDescriptorGetInfo(get_info);
	const size_t data_size = ::getDataSize(ctx, vk_get);
	VkDeviceSize binding_offset;
	vkGetDescriptorSetLayoutBindingOffsetEXT(ctx.Device, ds_layout, static_cast<uint32_t>(binding), &binding_offset);

	//compute layer offset
	//according to specification, descriptor array in a binding is tightly packed
	const VkDeviceSize layer_offset = layer * data_size;

	const VkDeviceSize update_offset = set_offset + binding_offset + layer_offset;
	vkGetDescriptorEXT(ctx.Device, &vk_get, data_size, buf_ptr + update_offset);

	//record flush information
	auto& [allocation, offset, size] = dmb.Flush;
	allocation.push_back(*dmb.DescriptorBuffer.first);
	offset.push_back(update_offset);
	size.push_back(data_size);
}

DescriptorBufferManager::DescriptorBufferManager() noexcept : UpdaterAlive(false) {

}

DescriptorBufferManager::DescriptorBufferManager(const VulkanContext& ctx,
	const span<const VkDescriptorSetLayout> ds_layout, const VkBufferUsageFlags usage) :
	Offset(ds_layout.size()), UpdaterAlive(false) {
	constexpr static auto roundUp = [](const size_t num, const size_t mul) constexpr noexcept -> size_t {
		//This round-up optimisation is designed for power-of-2 multiple,
		//and I would be surprised if the device alignment is not power-of-2.
		assert(std::has_single_bit(mul));
		return (num + mul - 1u) & ~(mul - 1u);
	};

	const span<VkDeviceSize> offset_span = this->Offset.toSpan();
	//use offset as a temporary storage for size
	std::ranges::transform(ds_layout, offset_span.begin(), [dev = *ctx.Device](const auto layout) {
		VkDeviceSize size;
		vkGetDescriptorSetLayoutSizeEXT(dev, layout, &size);
		return size;
	});
	//round size of all sets except the last one to multiple of alignment so that the next set starts at the correct offset
	std::ranges::for_each(offset_span | std::views::take(offset_span.size() - 1u),
		[alignment = ctx.PhysicalDeviceProperty.DescriptorBuffer.descriptorBufferOffsetAlignment](VkDeviceSize& size) {
			size = roundUp(size, alignment);
		});

	//compute the total size of all descriptor set layouts
	const VkDeviceSize total_size = std::reduce(offset_span.begin(), offset_span.end(), VkDeviceSize { 0 });
	this->DescriptorBuffer = BufferManager::createDescriptorBuffer({ ctx.Device, ctx.Allocator, total_size }, usage);

	//now convert from size to offset
	std::exclusive_scan(offset_span.begin(), offset_span.end(), offset_span.begin(), VkDeviceSize { 0 });
}

constexpr void DescriptorBufferManager::clearFlush() noexcept {
	auto& [allocation, offset, size] = this->Flush;
	allocation.clear();
	offset.clear();
	size.clear();
}

DescriptorBufferManager::DescriptorUpdater DescriptorBufferManager::createUpdater(const VulkanContext& ctx) {
	return DescriptorUpdater(ctx, *this);
}

VkBuffer DescriptorBufferManager::buffer() const noexcept {
	return this->DescriptorBuffer.second;
}

span<const VkDeviceSize> DescriptorBufferManager::offset() const noexcept {
	return this->Offset.toSpan();
}

VkDeviceSize DescriptorBufferManager::offset(const size_t index) const noexcept {
	return this->Offset[index];
}