#pragma once

#include <glad/gl.h>

#include <utility>

namespace rvo {
	class Framebuffer final {
	public:
		struct CreateInfo final {
			int _dummy;
		};

		constexpr Framebuffer() noexcept = default;
		Framebuffer(CreateInfo const& aInfo);
		Framebuffer(Framebuffer const&) = delete;
		Framebuffer& operator=(Framebuffer const&) = delete;
		Framebuffer(Framebuffer&& aOther) noexcept { *this = std::move(aOther); }
		Framebuffer& operator=(Framebuffer&& aOther) noexcept;
		~Framebuffer() noexcept;

		void bind() const;

		GLuint handle() const noexcept { return mHandle; }
	private:
		GLuint mHandle = 0;
	};
}