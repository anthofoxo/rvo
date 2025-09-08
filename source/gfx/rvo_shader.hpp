#pragma once

#include "../rvo_utility.hpp"

#include <glm/glm.hpp>
#include <glad/gl.h>

#include <vector>
#include <cstddef>
#include <utility>
#include <span>
#include <string_view>
#include <unordered_map>
#include <string>

namespace rvo {
	class Shader final {
	public:
		constexpr Shader() noexcept = default;

		Shader(GLenum aType, std::vector<std::byte> aSource)
			: Shader(aType, std::string_view(reinterpret_cast<char*>(aSource.data()), aSource.size())) {}

		Shader(GLenum aType, std::string_view aSource);

		Shader(Shader const&) = delete;
		Shader& operator=(Shader const&) = delete;
		Shader(Shader&& aOther) noexcept { *this = std::move(aOther); }
		Shader& operator=(Shader&& aOther) noexcept;

		~Shader() noexcept;

		GLuint handle() const noexcept { return mHandle; }

	private:
		GLuint mHandle = 0;
	};
	
	class ShaderProgram final {
	public:
		ShaderProgram() = default;

		ShaderProgram(std::span<std::reference_wrapper<Shader const> const> aShaders);

		ShaderProgram(std::initializer_list<std::reference_wrapper<Shader const>> aShaders)
			: ShaderProgram(std::span<std::reference_wrapper<Shader const> const>(aShaders.begin(), aShaders.end())) {}

		ShaderProgram(ShaderProgram const&) = delete;
		ShaderProgram& operator=(ShaderProgram const&) = delete;
		ShaderProgram(ShaderProgram&& aOther) noexcept { *this = std::move(aOther); }
		ShaderProgram& operator=(ShaderProgram&& aOther) noexcept;

		~ShaderProgram() noexcept;

		void push_1f(std::string_view aName, float aValue);
		void push_1i(std::string_view aName, int aValue);
		void push_2f(std::string_view aName, glm::vec2 const& aValue);
		void push_3f(std::string_view aName, glm::vec3 const& aValue);
		void push_mat4f(std::string_view aName, glm::mat4 const& aValue);

		void bind() const;
		GLint get_uniform_location(std::string_view aName) const;

		GLuint handle() const noexcept { return mHandle; }
	private:
		GLuint mHandle = 0;
		rvo::UnorderedStringMap<GLint> mUniformLocations;
	};
}