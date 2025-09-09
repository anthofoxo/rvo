#include "rvo_texture.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "../stb_image.h"

#include <spdlog/spdlog.h>

#include <cassert>
#include <limits>

#include <glm/glm.hpp>
#include <glm/gtc/integer.hpp>

namespace rvo {
	namespace {
		GLfloat gMaxAnisotropy = -1.0f;
	}

	Texture Texture::make_from_path(char const* aPath, bool aSrgb) {
		int x, y;
		stbi_uc* pixels = stbi_load(aPath, &x, &y, nullptr, 4);
		assert(pixels);

		int const numLevels = glm::log2(glm::max(x, y)) + 1;

		GLenum const internalFormat = aSrgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;

		Texture texture = { {
				.levels = numLevels,
				.internalFormat = internalFormat,
				.width = x,
				.height = y,
				.minFilter = GL_LINEAR_MIPMAP_LINEAR,
				.magFilter = GL_LINEAR,
				.wrap = GL_REPEAT,
				.anisotropy = std::numeric_limits<float>::infinity(), // Use max supported anisotropy
			} };

		texture.upload(x, y, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
		texture.generate_mipmaps();

		return texture;
	}

	Texture::Texture(CreateInfo const& aInfo) {
		if (gMaxAnisotropy < 0.0f) {
			glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &gMaxAnisotropy);
			spdlog::debug("GL_MAX_TEXTURE_MAX_ANISOTROPY: {}", gMaxAnisotropy);
		}

		glCreateTextures(GL_TEXTURE_2D, 1, &mHandle);
		glTextureStorage2D(mHandle, glm::max(aInfo.levels, 1), aInfo.internalFormat, aInfo.width, aInfo.height);
		glTextureParameteri(mHandle, GL_TEXTURE_MIN_FILTER, aInfo.minFilter);
		glTextureParameteri(mHandle, GL_TEXTURE_MAG_FILTER, aInfo.magFilter);
		glTextureParameteri(mHandle, GL_TEXTURE_WRAP_S, aInfo.wrap);
		glTextureParameteri(mHandle, GL_TEXTURE_WRAP_T, aInfo.wrap);
		glTextureParameteri(mHandle, GL_TEXTURE_WRAP_R, aInfo.wrap);
		glTextureParameterf(mHandle, GL_TEXTURE_MAX_ANISOTROPY, glm::min(aInfo.anisotropy, gMaxAnisotropy));
	}

	void Texture::upload(GLsizei width, GLsizei height, GLenum format, GLenum type, void const* pixels) const {
		glTextureSubImage2D(mHandle, 0, 0, 0, width, height, format, type, pixels);
	}

	void Texture::generate_mipmaps() const {
		glGenerateTextureMipmap(mHandle);
	}

	void Texture::bind(GLuint aUnit) const {
		glBindTextureUnit(aUnit, mHandle);
	}

	Texture& Texture::operator=(Texture&& aOther) noexcept {
		std::swap(mHandle, aOther.mHandle);
		return *this;
	}

	Texture::~Texture() noexcept {
		if (mHandle) {
			glDeleteTextures(1, &mHandle);
		}
	}
}