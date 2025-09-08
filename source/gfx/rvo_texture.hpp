#pragma once

#include <glad/gl.h>

#include <utility>

namespace rvo {
	class Texture final {
	public:
		struct CreateInfo final {
			GLsizei levels;
			GLenum internalFormat;
			GLsizei width;
			GLsizei height;
			GLenum minFilter;
			GLenum magFilter;
			GLenum wrap;
			GLfloat anisotropy;
		};

		static Texture make_from_path(char const* aPath, bool aSrgb = true);

		constexpr Texture() noexcept = default;
		Texture(CreateInfo const& aInfo);
		Texture(Texture const&) = delete;
		Texture& operator=(Texture const&) = delete;
		Texture(Texture&& aOther) noexcept { *this = std::move(aOther); };
		Texture& operator=(Texture&& aOther) noexcept;
		~Texture() noexcept;

		void upload(GLsizei width, GLsizei height, GLenum format, GLenum type, void const* pixels) const;
		void generate_mipmaps() const;

		void bind(GLuint aUnit) const;

		GLuint handle() const noexcept { return mHandle; }
	private:
		GLuint mHandle = 0;
	};

}