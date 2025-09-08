#pragma once

#include <vector>
#include <optional>
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

namespace rvo {
	template<typename T> auto size_bytes(T const& data) noexcept { return sizeof(typename T::value_type) * data.size(); }

	std::optional<std::vector<std::byte>> read_file_bytes(std::filesystem::path const& aPath);

	struct StringMultiHash final {
		using hash_type = std::hash<std::string_view>;
		using is_transparent = void;
		std::size_t operator()(char const* str) const { return hash_type{}(str); }
		std::size_t operator()(std::string_view str) const { return hash_type{}(str); }
		std::size_t operator()(std::string const& str) const { return hash_type{}(str); }
	};

	template<class V> using UnorderedStringMap = std::unordered_map<std::string, V, StringMultiHash, std::equal_to<>>;
}