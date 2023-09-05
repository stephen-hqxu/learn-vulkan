#pragma once

#include <memory>
#include <span>

#include <concepts>

#include <cassert>

namespace LearnVulkan {

	/**
	 * @brief A static array is an array allocated on heap with fixed size, thus the class is smaller than dynamic array class.
	 * The static array is built from a unique pointer, thus it is not copyable.
	 * @tparam T The type of the array. As the static array class utilises unique pointer,
	 * thus the type must be default constructable. Once should use std::vector if deferred construction is required.
	*/
	template<std::default_initializable T>
	class StaticArray {
	private:

		std::unique_ptr<T[]> Array;
		size_t Size;

	public:

		/**
		 * @brief Create a static array with no memory.
		*/
		constexpr StaticArray() noexcept : Size(0u) {
		
		}

		/**
		 * @brief Create an allocated static array with uninitialised content.
		 * @param size The size of the array.
		*/
		StaticArray(const size_t size) : Array(std::make_unique<T[]>(size)), Size(size) {

		}

		StaticArray(const StaticArray&) = delete;

		StaticArray(StaticArray&&) noexcept = default;

		StaticArray& operator=(const StaticArray&) = delete;

		StaticArray& operator=(StaticArray&&) noexcept = default;

		~StaticArray() = default;

		/**
		 * @brief Get the reference to the underlying object.
		 * @param index The index into the array.
		 * @return The reference to the object.
		*/
		T& operator[](const size_t index) noexcept {
			assert(index < this->Size);
			return this->Array[index];
		}

		/**
		 * @see operator[]
		*/
		const T& operator[](const size_t index) const noexcept {
			assert(index < this->Size);
			return this->Array[index];
		}

		/**
		 * @brief Get the underlying data pointer.
		 * @return The pointer to the underlying data.
		*/
		constexpr T* data() noexcept {
			return this->Array.get();
		}

		/**
		 * @see data()
		*/
		constexpr const T* data() const noexcept {
			return this->Array.get();
		}

		/**
		 * @brief Get the size of the array.
		 * @return The array size.
		*/
		constexpr size_t size() const noexcept {
			return this->Size;
		}

		/**
		 * @brief Create a span from the static array.
		 * @return The span.
		*/
		constexpr auto toSpan() {
			return std::span<T>(this->data(), this->size());
		}

		/**
		 * @brief Similarly, but using constant semantics.
		 * @see toSpan() 
		*/
		constexpr auto toSpan() const {
			return std::span<const T>(this->data(), this->size());
		}

	};

}