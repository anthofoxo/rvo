#pragma once

#include "rvo_gfx.hpp"
#include "rvo_utility.hpp"
#include "rvo_material.hpp"

#include <string>
#include <memory>
#include <unordered_map>

namespace rvo {
	class AssetManager final {
	public:
		std::shared_ptr<rvo::ShaderProgram> get_shader_program(std::string_view aSource);
		std::shared_ptr<rvo::Mesh> get_mesh(std::string_view aSource);
		std::shared_ptr<rvo::Texture> get_texture(std::string_view aSource);
		std::shared_ptr<rvo::Material> get_material(std::string_view aSource);
	public:
		rvo::UnorderedStringMap<std::shared_ptr<rvo::ShaderProgram>> mShaderPrograms;
		rvo::UnorderedStringMap<std::shared_ptr<rvo::Mesh>> mMeshes;
		rvo::UnorderedStringMap<std::shared_ptr<rvo::Texture>> mTextures;
		rvo::UnorderedStringMap<std::shared_ptr<rvo::Material>> mMaterials;
	};
}