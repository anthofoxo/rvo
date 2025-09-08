#include "rvo_shader.hpp"

#include <spdlog/spdlog.h>
#include <glm/gtc/type_ptr.hpp>

#ifdef _WIN32
#	include <Windows.h>
#endif

namespace rvo {
	Shader::Shader(GLenum aType, std::string_view aSource) {

		constexpr int numSources = 3;
		std::array<char const*, numSources> sources;
		std::array<GLint, numSources> lengths;

		sources[0] = "#version 460 core\n#extension GL_ARB_shading_language_include : require\n";
		lengths[0] = -1;

		switch (aType) {
		case GL_VERTEX_SHADER:
			sources[1] = "#define RVO_VERT\n";
			break;
		case GL_FRAGMENT_SHADER:
			sources[1] = "#define RVO_FRAG\n";
			break;
		default:
			sources[1] = "#define RVO_UNKN\n";
			break;
		}

		lengths[1] = -1;

		sources[2] = aSource.data();
		lengths[2] = aSource.size();

		mHandle = glCreateShader(aType);
		glShaderSource(mHandle, numSources, sources.data(), lengths.data());
		glCompileShader(mHandle);

		GLint logLength;
		glGetShaderiv(mHandle, GL_INFO_LOG_LENGTH, &logLength);

		if (logLength > 0) {
			std::vector<GLchar> data(logLength);

			glGetShaderInfoLog(mHandle, data.size(), nullptr, data.data());
			spdlog::error("{}", data.data());

#ifdef _WIN32
			if (IsDebuggerPresent()) __debugbreak();
#endif
		}
	}

	Shader& Shader::operator=(Shader&& aOther) noexcept {
		std::swap(mHandle, aOther.mHandle);
		return *this;
	}

	Shader::~Shader() noexcept {
		if (mHandle) {
			glDeleteShader(mHandle);
		}
	}

	ShaderProgram::ShaderProgram(std::span<std::reference_wrapper<Shader const> const> aShaders) {
		mHandle = glCreateProgram();

		for (auto const& shader : aShaders) {
			glAttachShader(mHandle, shader.get().handle());
		}

		glLinkProgram(mHandle);

		for (auto const& shader : aShaders) {
			glDetachShader(mHandle, shader.get().handle());
		}

		GLint logLength;
		glGetProgramiv(mHandle, GL_INFO_LOG_LENGTH, &logLength);

		if (logLength > 0) {
			std::vector<GLchar> data(logLength);

			glGetProgramInfoLog(mHandle, data.size(), nullptr, data.data());
			spdlog::error("{}", data.data());

#ifdef _WIN32
			if (IsDebuggerPresent()) __debugbreak();
#endif
		}

		// Fetch uniforms, avoid constantly looking up uniform locations

		GLint uniformCount = 0;
		glGetProgramiv(mHandle, GL_ACTIVE_UNIFORMS, &uniformCount);

		if (uniformCount > 0) {
			GLint maxNameLength;
			glGetProgramiv(mHandle, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxNameLength);

			auto uniformName = std::make_unique<GLchar[]>(maxNameLength);

			for (GLint i = 0; i < uniformCount; ++i) {
				GLsizei length;
				GLsizei count;
				GLenum type;

				glGetActiveUniform(mHandle, i, maxNameLength, &length, &count, &type, uniformName.get());
				mUniformLocations[std::string(uniformName.get())] = glGetUniformLocation(mHandle, uniformName.get());
			}
		}
	}
		
	ShaderProgram& ShaderProgram::operator=(ShaderProgram&& aOther) noexcept {
		std::swap(mHandle, aOther.mHandle);
		std::swap(mUniformLocations, aOther.mUniformLocations);
		return *this;
	}

	ShaderProgram::~ShaderProgram() noexcept {
		if (mHandle) {
			glDeleteProgram(mHandle);
		}
	}

	void ShaderProgram::bind() const {
		glUseProgram(mHandle);
	}

	void ShaderProgram::push_1f(std::string_view aName, float aValue) {
		glProgramUniform1f(mHandle, get_uniform_location(aName), aValue);
	}

	void ShaderProgram::push_1i(std::string_view aName, int aValue) {
		glProgramUniform1i(mHandle, get_uniform_location(aName), aValue);
	}

	void ShaderProgram::push_2f(std::string_view aName, glm::vec2 const& aValue) {
		glProgramUniform2fv(mHandle, get_uniform_location(aName), 1, glm::value_ptr(aValue));
	}

	void ShaderProgram::push_3f(std::string_view aName, glm::vec3 const& aValue) {
		glProgramUniform3fv(mHandle, get_uniform_location(aName), 1, glm::value_ptr(aValue));
	}

	void ShaderProgram::push_mat4f(std::string_view aName, glm::mat4 const& aValue) {
		glProgramUniformMatrix4fv(mHandle, get_uniform_location(aName), 1, GL_FALSE, glm::value_ptr(aValue));
	}

	GLint ShaderProgram::get_uniform_location(std::string_view aName) const {
		auto it = mUniformLocations.find(aName);
		if (it == mUniformLocations.end()) return -1;
		return it->second;
	}
}