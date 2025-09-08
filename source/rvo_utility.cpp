#include "rvo_utility.hpp"

#include <fstream>

namespace rvo {
	std::optional<std::vector<std::byte>> read_file_bytes(std::filesystem::path const& aPath) {
		std::ifstream stream(aPath, std::ios::binary);
		if (!stream) return std::nullopt;

		stream.seekg(0, std::ios::end);
		std::vector<std::byte> bytes(stream.tellg());
		stream.seekg(0, std::ios::beg);
		stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size());

		return bytes;
	}
}