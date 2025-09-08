#pragma once

#include "rvo_gfx.hpp"

#include <string>
#include <memory>
#include <unordered_map>

namespace rvo {
	class AssetManager final {
	public:
		std::shared_ptr<rvo::ShaderProgram> get_shader_program(std::string const& aSource);
		std::shared_ptr<rvo::Mesh> get_mesh(std::string const& aSource);
		std::shared_ptr<rvo::Texture> get_texture(std::string const& aSource);
	private:
		std::unordered_map<std::string, std::shared_ptr<rvo::ShaderProgram>> mShaderPrograms;
		std::unordered_map<std::string, std::shared_ptr<rvo::Mesh>> mMeshes;
		std::unordered_map<std::string, std::shared_ptr<rvo::Texture>> mTextures;
	};
}