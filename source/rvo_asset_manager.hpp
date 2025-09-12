#pragma once

#include "rvo_gfx.hpp"
#include "rvo_utility.hpp"
#include "rvo_material.hpp"

#include <string>
#include <memory>
#include <unordered_map>
#include <filesystem>

namespace rvo {
	class AssetManager final {
	public:
		template<typename T>
		struct AssetReference final {
			std::weak_ptr<T> asset;
			std::filesystem::file_time_type lastWriteTime;
		};

		std::shared_ptr<rvo::ShaderProgram> get_shader_program(std::string_view aSource);
		std::shared_ptr<rvo::Mesh> get_mesh(std::string_view aSource);
		std::shared_ptr<rvo::Texture> get_texture(std::string_view aSource);
		std::shared_ptr<rvo::Material> get_material(std::string_view aSource);

		void update();
	public:
		rvo::UnorderedStringMap<AssetReference<rvo::ShaderProgram>> mShaderPrograms;
		rvo::UnorderedStringMap<AssetReference<rvo::Mesh>> mMeshes;
		rvo::UnorderedStringMap<AssetReference<rvo::Texture>> mTextures;
		rvo::UnorderedStringMap<AssetReference<rvo::Material>> mMaterials;
	};
}