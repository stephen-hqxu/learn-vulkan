#pragma once

#include <array>
#include <utility>
#include <type_traits>
#include <cassert>

namespace LearnVulkan {

	/**
	 * @brief Fixed array is an array with compile-time size limit, but behaves like a dynamic array.
	 * However, the dynamic size should not exceed the size limit.
	 * @tparam T The type of the array. Must be trivial.
	 * @tparam N The compile-time size limit of the array.
	*/
	template<class T, size_t N>
	requires(std::is_trivial_v<T> && std::is_trivially_destructible_v<T>)
	class FixedArray {
	private:

		std::array<T, N> Array;
		size_t Size;

	public:

		constexpr FixedArray() noexcept : Size(0u) {
		
		}

		~FixedArray() = default;

		/**
		 * @brief Get the maximum size of the array.
		 * @return The maximum size of the array.
		*/
		consteval size_t capacity() const noexcept {
			return N;
		}

		/**
		 * @brief Get the current size of the array.
		 * @return The current size.
		*/
		constexpr size_t size() const noexcept {
			return this->Size;
		}

		/**
		 * @brief Get the raw pointer into the array.
		 * @return The raw pointer.
		*/
		constexpr T* data() noexcept {
			return this->Array.data();
		}
		constexpr const T* data() const noexcept {
			return this->Array.data();
		}

		/**
		 * @brief Get the reference to the underlying element given an index.
		 * @param index The index into the array.
		 * @return The reference to the element in the array.
		*/
		constexpr T& operator[](const size_t index) noexcept {
			assert(index < this->Size);
			return this->Array[index];
		}
		constexpr const T& operator[](const size_t index) const noexcept {
			assert(index < this->Size);
			return this->Array[index];
		}

		//Similar to standard library container.
		constexpr T& pushBack(T&& object) noexcept {
			assert(this->Size < N);
			return this->Array[this->Size++] = std::forward<T>(object);
		}

		/**
		 * @brief Clear the array.
		*/
		constexpr void clear() noexcept {
			//because the type is trivially destructible,
			//we can simply move the pointer back to the beginning without calling each of their destructors. 
			this->Size = 0u;
		}

	};

}