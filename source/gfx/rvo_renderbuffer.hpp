#pragma once

#include <glad/gl.h>

#include <utility>

namespace rvo {
	class Renderbuffer final {
	public:
		struct CreateInfo final {
			GLenum internalFormat;
			GLsizei width;
			GLsizei height;
		};

		constexpr Renderbuffer() noexcept = default;
		Renderbuffer(CreateInfo const& aInfo);
		Renderbuffer(Renderbuffer const&) = delete;
		Renderbuffer& operator=(Renderbuffer const&) = delete;
		Renderbuffer(Renderbuffer&& aOther) noexcept { *this = std::move(aOther); }
		Renderbuffer& operator=(Renderbuffer&& aOther) noexcept;
		~Renderbuffer() noexcept;

		GLuint handle() const noexcept { return mHandle; }
	private:
		GLuint mHandle = 0;
	};
}