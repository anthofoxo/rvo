#pragma once

#include <glad/gl.h>

#include <utility>
#include <span>

namespace rvo {
	class Buffer final {
	public:
		struct CreateInfo final {
			std::span<std::byte const> data;
			GLbitfield flags;
		};

		constexpr Buffer() noexcept = default;
		Buffer(CreateInfo const& aInfo);
		Buffer(Buffer const&) = delete;
		Buffer& operator=(Buffer const&) = delete;
		Buffer(Buffer&& aOther) noexcept { *this = std::move(aOther); }
		Buffer& operator=(Buffer&& aOther) noexcept;
		~Buffer() noexcept;

		GLuint handle() const noexcept { return mHandle; }
	private:
		GLuint mHandle = 0;
	};
}