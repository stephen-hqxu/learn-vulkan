#pragma once

#include <iterator>
#include <type_traits>
#include <utility>
#include <cassert>

namespace LearnVulkan {

	/**
	 * @brief A span array is a non-owning contiguous storage which allows expansion.
	 * @tparam T The type of the containing data.
	*/
	template<class T>
	requires(std::is_trivial_v<T> && std::is_trivially_destructible_v<T>)
	class SpanArray {
	private:

		T* Begin, *End, *Capacity;

	public:

		/**
		 * @brief Initialise a span array with no capacity.
		*/
		constexpr SpanArray() noexcept : Begin(nullptr), End(nullptr), Capacity(nullptr) {
		
		}

		/**
		 * @brief Initialise a span array with capacity from a pointer and a range.
		 * @param begin The begin pointer.
		 * @param count The number of element.
		*/
		constexpr SpanArray(T* const begin, const size_t count) noexcept : Begin(begin), End(this->Begin), Capacity(begin + count) {
		
		}

		~SpanArray() = default;

		/**
		 * @brief Get the size of the span array.
		 * @return The number of element currently holding.
		*/
		constexpr size_t size() const noexcept {
			return static_cast<size_t>(std::distance(this->Begin, this->End));
		}

		/**
		 * @brief Get the total capacity of the span array.
		 * @return The total number of element can hold.
		*/
		constexpr size_t capacity() const noexcept {
			return static_cast<size_t>(std::distance(this->Begin, this->Capacity));
		}

		/**
		 * @brief Get the pointer into the underlying storage.
		 * @return The storage data pointer.
		*/
		constexpr T* data() noexcept {
			return this->Begin;
		}
		constexpr const T* data() const noexcept {
			return this->Begin;
		}

		/**
		 * @brief Get the underlying element.
		 * @param idx The index into the storage.
		 * @return The element at the given index.
		*/
		constexpr T& operator[](const size_t idx) noexcept {
			assert(idx < this->size());
			return this->Begin[idx];
		}
		constexpr const T& operator[](const size_t idx) const noexcept {
			assert(idx < this->size());
			return this->Begin[idx];
		}

		/**
		 * @brief Push back an object into the span array.
		 * @param object The object to be added.
		 * @return The reference to the newly added object into the container..
		*/
		constexpr T& pushBack(T&& object) noexcept {
			assert(this->End != this->Capacity);

			T& inserted = *this->End;
			inserted = std::forward<T>(object);
			this->End++;
			return inserted;
		}

		/**
		 * @brief Clear the container.
		*/
		constexpr void clear() noexcept {
			this->End = this->Begin;
		}

	};

}