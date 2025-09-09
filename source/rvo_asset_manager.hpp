#pragma once

#include "rvo_gfx.hpp"
#include "rvo_utility.hpp"

#include <string>
#include <memory>
#include <unordered_map>

namespace rvo {
	class AssetManager final {
	public:
		std::shared_ptr<rvo::ShaderProgram> get_shader_program(std::string_view aSource);
		std::shared_ptr<rvo::Mesh> get_mesh(std::string_view aSource);
		std::shared_ptr<rvo::Texture> get_texture(std::string_view aSource);
	private:
		rvo::UnorderedStringMap<std::shared_ptr<rvo::ShaderProgram>> mShaderPrograms;
		rvo::UnorderedStringMap<std::shared_ptr<rvo::Mesh>> mMeshes;
		rvo::UnorderedStringMap<std::shared_ptr<rvo::Texture>> mTextures;
	};
}