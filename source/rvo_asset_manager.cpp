#include "rvo_asset_manager.hpp"

#include "rvo_utility.hpp"

#define STB_INCLUDE_LINE_GLSL
#define STB_INCLUDE_IMPLEMENTATION
#include "stb_include.h"

namespace rvo {
	std::shared_ptr<rvo::ShaderProgram> AssetManager::get_shader_program(std::string const& aSource) {
		auto it = mShaderPrograms.find(aSource);
		if (it != mShaderPrograms.end()) return it->second;

		auto optBytes = rvo::read_file_string(aSource);
		if (!optBytes) return nullptr;

		char error[256];
		char* vertSource = stb_include_string(optBytes->c_str(), "#version 460 core\n#define RVO_VERT\n", "shaders/include", aSource.c_str(), error);
		char* fragSource = stb_include_string(optBytes->c_str(), "#version 460 core\n#define RVO_FRAG\n", "shaders/include", aSource.c_str(), error);

		auto vert = rvo::Shader(GL_VERTEX_SHADER, vertSource);
		auto frag = rvo::Shader(GL_FRAGMENT_SHADER, fragSource);

		free(vertSource);
		free(fragSource);

		auto ref = std::make_shared<rvo::ShaderProgram>(rvo::ShaderProgram({ vert, frag }));
		mShaderPrograms[aSource] = ref;
		return ref;
	}

	std::shared_ptr<rvo::Mesh> AssetManager::get_mesh(std::string const& aSource) {
		auto it = mMeshes.find(aSource);
		if (it != mMeshes.end()) return it->second;

		auto optBytes = rvo::read_file_bytes(aSource);
		if (!optBytes) return nullptr;

		auto ref = std::make_shared<rvo::Mesh>(aSource.c_str());
		mMeshes[aSource] = ref;
		return ref;
	}

	std::shared_ptr<rvo::Texture> AssetManager::get_texture(std::string const& aSource) {
		auto it = mTextures.find(aSource);
		if (it != mTextures.end()) return it->second;

		auto ref = std::make_shared<rvo::Texture>(rvo::Texture::make_from_path(aSource.c_str()));
		mTextures[aSource] = ref;
		return ref;
	}
}