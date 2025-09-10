#pragma once

#include "rvo_buffer.hpp"

#include <glm/glm.hpp>
#include <glad/gl.h>

#include <utility>

namespace rvo {
	GLuint get_instanced_buffer();

	struct StandardVertex final {
		glm::vec3 position;
		glm::vec3 normal;
		glm::vec2 textureCoord;
	};

	class Mesh final {
	public:
		constexpr Mesh() noexcept = default;

		Mesh(char const* aPath);
		Mesh(Mesh const&) = delete;
		Mesh& operator=(Mesh const&) = delete;
		Mesh(Mesh&& aOther) noexcept { *this = std::move(aOther); }
		Mesh& operator=(Mesh&& aOther) noexcept;
		~Mesh() noexcept;

		void bind() const;
		void draw(GLsizei aInstanceCount = 1) const;
		[[deprecated]] void render() const;

		GLuint vao() const noexcept { return mVao; }

	private:
		GLuint mVao = 0;
		Buffer mVbo, mEbo;
		GLsizei mCount = 0;
	};
}