#pragma once

#include <string>
#include <string_view>
#include <array>
#include <tuple>

#include <utility>

namespace LearnVulkan {

	/**
	 * @brief Handle file read and write.
	*/
	namespace File {

		/**
		 * @brief Read whole content from the target file formatted as string.
		 * @param filename The name of file.
		 * @return String content in the file.
		 * @exception If file cannot be opened/read.
		*/
		std::string readString(const char*);

		/**
		 * @brief Join two string literals in compile time.
		 * @tparam RightString The string value on the RHS.
		 * @tparam LeftString The string value on the LHS.
		 * @return An array of null-terminated characters.
		*/
		template<const std::string_view& LeftString, const std::string_view& RightString>
		consteval auto joinString() noexcept {
			using std::make_index_sequence;
			using std::index_sequence;
			//BUG: A bug in MSVC for not recognising the using scope in lambda template parameter, I have reported it.
			//Remove std:: prefix and instead using `using` when it is fixed.
			constexpr auto makeString =
				[]<size_t... IL, size_t... IR>(std::index_sequence<IL...>, std::index_sequence<IR...>) consteval noexcept -> auto {
				return std::array { LeftString[IL]..., RightString[IR]..., '\0' };
			};
			return makeString(make_index_sequence<LeftString.size()> { }, make_index_sequence<RightString.size()> { });
		}

		/**
		 * @brief Convert a relative filename to its absolute path representation.
		 * Either the prefix should include a trailing filename separator, or the filename should include a leading separator.
		 * @tparam Prefix The path to be prefixed.
		 * @tparam ...Filename A list of filename to be generated.
		 * @return An array or a tuple of each input filename in absolute path form.
		 * If the input filenames all have the same length, an array is returned.
		*/
		template<const std::string_view& Prefix, const std::string_view&... Filename>
		consteval auto toAbsolutePath() noexcept {
			constexpr auto all_same_size = [](const auto x, const auto... xs) consteval noexcept -> bool {
				return ((x == xs) && ... && true);
			};

			//If the resulting size is the same, use array, otherwise tuple.
			if constexpr (all_same_size(Filename.size()...)) {
				return std::array { joinString<Prefix, Filename>()... };
			} else {
				return std::make_tuple(joinString<Prefix, Filename>()...);
			}
		}

		/**
		 * @brief Convert an array of raw characters to string view.
		 * This overload resolves strings with equal length.
		 * @param StringCount The number of raw strings.
		 * @param RawStringSize The number of character in each string.
		 * @return An array of string view.
		*/
		template<size_t StringCount, size_t RawStringSize>
		consteval auto batchRawStringToView(const std::array<std::array<char, RawStringSize>, StringCount>& raw_array) noexcept {
			constexpr auto sequenceToView = []<size_t... I>(const auto& raw_array, std::index_sequence<I...>) consteval noexcept -> auto {
				//remove the trailing null character from the view
				return std::array { std::string_view(raw_array[I].data(), raw_array[I].size() - 1u)... };
			};
			return sequenceToView(raw_array, std::make_index_sequence<StringCount> { });
		}

	}

}